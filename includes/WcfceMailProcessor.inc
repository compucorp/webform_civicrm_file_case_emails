<?php

/**
 * @file
 * Webform submission mail processor.
 */

class WcfceMailProcessor {

  /**
   * Email data (comes from hook_mail_alter()).
   *
   * @var array
   */
  private $message;

  /**
   * WcfceMailProcessor constructor.
   *
   * @param array $message
   *   Email data (comes from hook_mail_alter()).
   */
  public function __construct(array $message) {
    $this->message = $message;
  }

  /**
   * File email as activity for all cases related to the webform.
   *
   * @return array|false
   *   Entity data of the last activity added, empty array or FALSE
   *   in case of error.
   *
   * @throws \CiviCRM_API3_Exception
   */
  public function createActivity() {
    if (!$this->isMessageValid()) {
      return FALSE;
    }

    if (!civicrm_initialize()) {
      return FALSE;
    }

    $assignee_id = $this->getContactIdByEmail($this->message['to']);
    $creator_id  = $this->getContactIdByEmail($this->message['from']);

    if (!$creator_id) {
      return FALSE;
    }

    $eml = $this->composeEml();
    $cases = &$this->message['params']['submission']->civicrm['case'];

    // For all cases related to the webform:
    foreach ($cases as $case) {
      // Create activity.
      $result = civicrm_api3('Activity', 'create', [
        'sequential' => 1,
        'activity_type_id' => 'Email',
        'case_id' => $case['id'],
        'subject' => $this->message['subject'],
        'details' => trim($this->message['body'][0]),
        'assignee_id' => $assignee_id,
        'source_contact_id' => $creator_id,
      ]);
      // Attach eml file to it.
      if (!empty($result['values'][0]['id'])) {
        $this->attachEmlToActivity($result['values'][0]['id'], $eml);
      }
    }

    return $result['values'][0] ?? [];
  }

  /**
   * Attach eml file to activity.
   *
   * @param int $activity_id
   *   Activity id.
   * @param string $eml
   *   Contents of eml file.
   * @param array|NULL $message
   *   Email data. If is set to NULL then $this->message would be used.
   *
   * @return array|false
   *   Attached file entity data, empty array or FALSE in case of error.
   *
   * @throws \CiviCRM_API3_Exception
   */
  public function attachEmlToActivity($activity_id, $eml, $message = NULL) {
    if (!$activity_id) {
      return FALSE;
    }

    $result = civicrm_api3('Attachment', 'create', [
      'sequential' => 1,
      'mime_type' => 'message/rfc822',
      'entity_table' => 'civicrm_activity',
      'entity_id' => $activity_id,
      'content' => $eml,
      'name' => $this->generateEmlFileName($message),
    ]);

    // Set file type to attached file.
    // (The file_type_id db table column is always NULL and not used in civicrm
    // at all. But we need to set some flag to differentiate original email from
    // other attached files (and disable edit/delete for this file later), so we
    // will use that field for this purpose and just set 1 as a value)
    if (!empty($result['values'][0]['id'])) {
      civicrm_api3('File', 'create', [
        'id' => $result['values'][0]['id'],
        'file_type_id' => 1,
      ]);
    }

    return $result['values'][0] ?? [];
  }

  /**
   * Generate name for eml file.
   *
   * @param array|NULL $message
   *   Email data. If is set to NULL then $this->message would be used.
   *
   * @return string
   */
  public function generateEmlFileName($message = NULL) {
    $message = $message ?? $this->message;
    $res = date('Ymd_Hi_')
      . preg_replace('!\W+!', '', trim(strtolower($message['subject'])))
      . '.eml';

    return $res;
  }

  /**
   * Check if email is a case webform submission message.
   *
   * @return bool
   *   FALSE if not, TRUE otherwise.
   */
  public function isMessageValid() {
    $res = TRUE;

    // Process webform submission emails only.
    if ($this->message['module'] !== 'webform' || $this->message['key'] !== 'submission') {
      $res = FALSE;
    }
    // Do process if a webform is configured to create/update a Case.
    elseif (empty($this->message['params']['submission']->civicrm['case'])) {
      $res = FALSE;
    }

    return $res;
  }

  /**
   * Generate eml file from email data.
   *
   * @param array|NULL $message
   *   Email data. If is set to NULL then $this->message would be used.
   *
   * @return string
   *   Eml file contents.
   *
   * @throws \Exception
   */
  public function composeEml($message = NULL) {
    $message = $message ?? $this->message;

    // (Copied from drupal_mail())
    // Retrieve the responsible implementation for this message.
    $system = drupal_mail_system($message['module'], $message['key']);
    // Format the message body.
    $message = $system->format($message);

    // (Copied from mimemail_mailengine())
    // Prepare email data.
    $crlf = variable_get('mimemail_crlf', MAIL_LINE_ENDINGS);
    $recipients = (!is_array($message['to'])) ? array($message['to']) : $message['to'];
    $subject = mime_header_encode($message['subject']);
    $body = preg_replace('@\r?\n@', $crlf, $message['body']);
    $headers = mimemail_rfc_headers($message['headers']);

    // Get email pieces together as eml file.
    $eml  = $headers . $crlf;
    $eml .= 'Date: ' . date(DATE_RFC822) . $crlf;
    $eml .= 'To: ' . implode($crlf . 'To: ', $recipients) . $crlf;
    $eml .= 'Subject: ' . $subject . $crlf . $crlf;
    $eml .= $body;

    return $eml;
  }

  /**
   * Get contact id by email address.
   *
   * @param string $email
   *   Email address to search by.
   *
   * @return int
   *   Civicrm contact id, or 0 if not found.
   *
   * @throws \CiviCRM_API3_Exception
   */
  public function getContactIdByEmail($email) {
    // Make sure email is plain email address and not something like
    // Hercules Poirot <hercules@mail.com>.
    $rfc = new Mail_RFC822;
    $email = $rfc->parseAddressList($email);
    $email = !empty($email[0]) ? $email[0]->mailbox . '@' . $email[0]->host : '';

    $result = civicrm_api3('Email', 'get', [
      'sequential' => 1,
      'return' => 'contact_id',
      'email' => $email,
    ]);

    return $result['values'][0]['contact_id'] ?? 0;
  }

}
