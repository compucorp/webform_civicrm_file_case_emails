<?php

/**
 * Class WcfceMailProcessor.
 *
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
   *   An exception generated by civicrm_api3().
   */
  public function createActivity() {
    if (!$this->isMessageValid() || !civicrm_initialize()) {
      return FALSE;
    }

    $assignee_id = $this->getContactIdByEmail($this->message['to']);
    $creator_id  = $this->getContactIdByEmail($this->message['from']);

    if (!$creator_id) {
      return FALSE;
    }

    $eml = $this->generateEml();
    $cases = $this->message['params']['submission']->civicrm['case'];

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

      if (!empty($result['id'])) {
        // Attach webform files to activity.
        $this->attachWebformFilesToActivity($result['id']);
        // Attach eml file to activity.
        $this->attachEmlToActivity($result['id'], $eml);
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
   * @param array|null $message
   *   (Optional) Email data. If set to NULL then $this->message would be used.
   *
   * @return array|false
   *   Attached file entity data, empty array or FALSE in case of error.
   *
   * @throws \CiviCRM_API3_Exception
   *   An exception generated by civicrm_api3().
   */
  private function attachEmlToActivity($activity_id, $eml, $message = NULL) {
    if (!$activity_id) {
      return FALSE;
    }

    $result = civicrm_api3('Attachment', 'create', [
      'sequential' => 1,
      'entity_table' => 'civicrm_activity',
      'entity_id' => $activity_id,
      'mime_type' => 'message/rfc822',
      'name' => $this->generateEmlFileName($message),
      'content' => $eml,
    ]);

    // Set file type to attached file.
    // (The file_type_id db table column is always NULL and not used in civicrm
    // at all. But we need to set some flag to differentiate original email from
    // other attached files (and disable edit/delete for this file later), so we
    // will use that field for this purpose and just set 1 as a value)
    if (!empty($result['id'])) {
      civicrm_api3('File', 'create', [
        'id' => $result['id'],
        'file_type_id' => 1,
      ]);
    }

    return $result['values'][0] ?? [];
  }

  /**
   * Generate name for eml file.
   *
   * @param array|null $message
   *   (Optional) Email data. If set to NULL then $this->message would be used.
   *
   * @return string
   *   File name (including extension) for original message eml file.
   */
  private function generateEmlFileName($message = NULL) {
    $message = $message ?? $this->message;
    $res = date('Ymd_Hi_')
      . preg_replace('!\W+!', '', trim(strtolower($message['subject'])));
    // Avoid too long file names.
    $res = substr($res, 0, 128) . '.eml';

    return $res;
  }

  /**
   * Check if email is a case webform submission message.
   *
   * @return bool
   *   FALSE if not, TRUE otherwise.
   */
  private function isMessageValid() {
    // Process webform submission emails only.
    if ($this->message['module'] !== 'webform' || $this->message['key'] !== 'submission') {
      return FALSE;
    }
    // Do process if a webform is configured to create/update a Case.
    if (empty($this->message['params']['submission']->civicrm['case'])) {
      return FALSE;
    }

    return TRUE;
  }

  /**
   * Generate eml file from email data.
   *
   * @param array|null $message
   *   (Optional) Email data. If set to NULL then $this->message would be used.
   *
   * @return string
   *   Eml file contents.
   */
  private function generateEml($message = NULL) {
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
    $eml = $this->appendWebformFilesToEml($eml, $message, $crlf);

    return $eml;
  }

  /**
   * Appends webform files as attachments to email text.
   *
   * @param string $eml
   *   Email message in eml format.
   * @param array|null $message
   *   (Optional) Email data. If set to NULL then $this->message would be used.
   * @param string $crlf
   *   (Optional) End of line character.
   *
   * @return string
   *   Email text (eml) with attachments.
   */
  private function appendWebformFilesToEml($eml, $message = NULL, $crlf = MAIL_LINE_ENDINGS) {
    $attachments = $this->getWebformFilesAsEml($eml, $message, $crlf);

    if (empty($attachments)) {
      return $eml;
    }

    $boundary = $this->getEmailBoundary($eml);
    // Eml final boundary looks like '--[boundary]--', but since we are adding
    // attachments this boundary is not final now, hence we need to remove
    // '--' at the end of the text (it would be added at the end of the files).
    $eml = rtrim(preg_replace('!--' . $boundary . '--!', '--' . $boundary, $eml))
      . $crlf
      . rtrim($attachments) . '--';

    return $eml;
  }

  /**
   * Attaches files attached to a webform submission to activity.
   *
   * @param int $activity_id
   *   Activity id.
   *
   * @return array|false
   *   Entity data of the last file added, empty array or FALSE
   *   in case of error.
   *
   * @throws \CiviCRM_API3_Exception
   *   An exception generated by civicrm_api3().
   */
  private function attachWebformFilesToActivity($activity_id) {
    if (!$activity_id) {
      return FALSE;
    }

    $file_ids = $this->getWebformFileIds($this->message);

    if (empty($file_ids)) {
      return FALSE;
    }

    foreach ($file_ids as $file_id) {
      $file = $this->getWebformFileObject($file_id);
      $result = civicrm_api3('Attachment', 'create', [
        'sequential' => 1,
        'entity_table' => 'civicrm_activity',
        'entity_id' => $activity_id,
        'mime_type' => $file->filemime,
        'name' => $file->filename,
        'content' => $this->getWebformFileContent($file_id, $file),
      ]);
    }

    return $result['values'][0] ?? [];
  }

  /**
   * Encodes attached file contents to append them to email.
   *
   * @param string $headers
   *   Email headers (used to get email boundary).
   * @param array|null $message
   *   (Optional) Email data. If set to NULL then $this->message would be used.
   * @param string $crlf
   *   (Optional) End of line character.
   *
   * @return string
   *   Attached file contents (encoded), or empty string if no files found.
   */
  private function getWebformFilesAsEml($headers, $message = NULL, $crlf = MAIL_LINE_ENDINGS) {
    $message = $message ?? $this->message;
    $res = '';

    // Check if files attached to a webform submission.
    $file_ids = $this->getWebformFileIds($message);

    if (empty($file_ids)) {
      return $res;
    }

    $boundary = $crlf . '--' . $this->getEmailBoundary($headers) . $crlf;

    // Attach files to email.
    foreach ($file_ids as $file_id) {
      $res .= $this->getWebformFileContentEml($file_id, NULL, $crlf);
      $res .= $boundary;
    }

    return $res;
  }

  /**
   * Get ids of files attached to submitted webform.
   *
   * @param array|null $message
   *   (Optional) Email data. If set to NULL then $this->message would be used.
   *
   * @return array
   *   Ids of files attached to a webform submission, or empty array if no files
   *   were attached.
   *   The result array looks like: array(webform_field_num => file_id, ...).
   */
  private function getWebformFileIds($message = NULL) {
    $message = $message ?? $this->message;
    $fields = $message['params']['node']->webform['components'];
    $values = $message['params']['submission']->data;
    $res = [];

    // Search for file type fields.
    foreach ($fields as $num => $field) {
      if ($field['type'] == 'file' && !empty($values[$num][0])) {
        // Get id of uploaded file.
        $res[$num] = $values[$num][0];
      }
    }

    return $res;
  }

  /**
   * Loads webform file.
   *
   * @param int $file_id
   *   Drupal file id that is attached to a webform submission.
   *
   * @return object
   *   Drupal file object, or NULL if not found.
   */
  private function getWebformFileObject($file_id) {
    return $file_id ? file_load($file_id) : NULL;
  }

  /**
   * Loads webform file content.
   *
   * @param int $file_id
   *   Drupal file id that is attached to a webform submission.
   * @param object $file
   *   (Optional) Drupal file object. If set then $file_id will be skipped.
   *
   * @return string
   *   File contents, or empty string if not found.
   */
  private function getWebformFileContent($file_id, $file = NULL) {
    $file = $file ?? $this->getWebformFileObject($file_id);
    return $file ? file_get_contents(drupal_realpath($file->uri)) : '';
  }

  /**
   * Loads webform file contents and encodes them to eml format.
   *
   * @param int $file_id
   *   Drupal file id that is attached to a webform submission.
   * @param object $file
   *   (Optional) Drupal file object. If set then $file_id will be skipped.
   * @param string $crlf
   *   (Optional) End of line character, used for encoding to eml.
   *
   * @return string
   *   Encoded file contents to be used in eml, or empty string if not found.
   */
  private function getWebformFileContentEml($file_id, $file = NULL, $crlf = MAIL_LINE_ENDINGS) {
    $file = $file ?? $this->getWebformFileObject($file_id);

    if (empty($file)) {
      return '';
    }

    $headers  = "Content-Type: " . $file->filemime . "; name=" . $file->filename . $crlf;
    $headers .= "Content-Disposition: attachment; filename=" . $file->filename . $crlf;
    $headers .= "Content-Transfer-Encoding: base64" . $crlf;
    $headers .= "X-Attachment-Id: " . rand(1000, 99999) . $crlf . $crlf;
    $res = $this->getWebformFileContent($file_id, $file);
    $res = $headers . chunk_split(base64_encode($res));

    return $res;
  }

  /**
   * Extracts email boundary from email headers.
   *
   * Email boundary is an md5 hash which is used to separate email parts from
   * each other (e.g. text and attached files).
   *
   * @param string $headers
   *   Email headers or whole email message.
   *
   * @return string
   *   Email boundary, or empty string if not found.
   */
  private function getEmailBoundary($headers) {
    preg_match('!boundary=\"(.+)\"!', $headers, $matches);
    return $matches[1] ?? '';
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
   *   An exception generated by civicrm_api3().
   */
  private function getContactIdByEmail($email) {
    // Make sure email is plain email address and not something like
    // Hercules Poirot <hercules@mail.com>.
    $rfc = new Mail_RFC822();
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