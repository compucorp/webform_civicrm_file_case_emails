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

## Usage

This module affects all webforms that are configured:
1. For CiviCRM processing.
1. To create/update a Case.
1. To send an email (notification).

Check if you've configured a webform properly:
1. Make sure that contact with `From` address exists. Civicrm would not create an activity if no matching contact is found (creator is required).
1. Webform module tends to send emails using site default email (see *Site information* page - */admin/config/system/site-information*) as `From` address, despite that actual `From` address is set in the webform configuration. So you may want to set address of some existing contact here or create new contact for that.
1. If you need to have `To` contact in new activity (it would be assigned as `With` contact), ensure that contact with this address exists.

## Credit

The development of this module is sponsored
by [Compucorp](https://www.compucorp.co.uk/).
