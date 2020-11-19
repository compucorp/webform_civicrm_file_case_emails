# Webform CiviCRM File Case Emails

On each webform submission this module would create an activity linked to a case
and attach original notification email as `.eml` file to it. So user would be
able to download and open this file with some email application
(e.g: Thunderbird, Outlook) to view all details of the original email. This may
be useful in some legal cases for example.

## Installation

To install this module, place it in your sites/all/modules folder and enable it
on the modules page as usual.

## Configuration

This module has no configuration - just enable it and enjoy.

However, you need to check if mail system settings are configured properly:
1. Go to Configuration -> System -> Mail System page
(*/admin/config/system/mailsystem*).
1. Make sure that *Site-wide default MailSystemInterface class* field has
*MimeMailSystem* value selected.

## Usage

This module affects all webforms that are configured:
1. For CiviCRM processing.
1. To create/update a Case.
1. To send an email (notification).

Check if you've configured a webform properly:
1. Make sure contact with `From` address exists. Civicrm would not create
an activity if no matching contact is found (creator is required).
1. Webform module tends to send emails using site default email (see *Site
information* page - */admin/config/system/site-information*) as `From` address,
despite that actual `From` address is set in the webform configuration. So you
may want to set address of some existing contact here or create new contact
for that.
1. If you need to have `To` contact in new activity (it would be assigned as
`With` contact), ensure that contact with this address exists.

### Multiple contacts with the same email

If you have multiple contacts with the same email, there could be a confusion:
as contacts are retrieved by email when activity is created, some other contact
may be accidentally used. Keep in mind that this module would try to search for
a contact that has given email as a primary email first to minimize confusions.

So avoiding duplicate emails is still the best solution to the problem.

However, there is a bonus point. As yo may know it is possible to configure
a webform notification to get `From`/`To` email address from some form field.
If this field comes from civicrm contact that you've configured under the
*Civicrm* tab of the form then it is possible to link email address to a contact
and this module would find correct contact any way.
Keep in mind: in order to make it work for hidden fields you need to make
sure it's properly configured: go to field settings and check if 'Hidden type'
is set to 'Hidden element' (not 'Secure value').

## Credit

The development of this module is sponsored
by [Compucorp](https://www.compucorp.co.uk/).
