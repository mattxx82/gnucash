/*
 * dialog-customer.c -- Dialog for Customer entry
 * Copyright (C) 2001 Derek Atkins
 * Author: Derek Atkins <warlord@MIT.EDU>
 */

#include "config.h"

#include <gnome.h>

#include "dialog-utils.h"
#include "global-options.h"
#include "gnc-amount-edit.h"
#include "gnc-component-manager.h"
#include "gnc-ui.h"
#include "gnc-gui-query.h"
#include "gnc-ui-util.h"
#include "gnc-engine-util.h"
#include "window-help.h"

#include "gncBusiness.h"
#include "gncAddress.h"
#include "gncCustomer.h"
#include "gncCustomerP.h"

#include "dialog-customer.h"
#include "business-chooser.h"

#define DIALOG_NEW_CUSTOMER_CM_CLASS "dialog-new-customer"
#define DIALOG_EDIT_CUSTOMER_CM_CLASS "dialog-edit-customer"

typedef enum
{
  NEW_CUSTOMER,
  EDIT_CUSTOMER
} CustomerDialogType;

struct _customer_select_window {
  GncBusiness *	business;
};

typedef struct _customer_window {
  GtkWidget *	dialog;

  GtkWidget *	id_entry;
  GtkWidget *	company_entry;

  GtkWidget *	name_entry;
  GtkWidget *	addr1_entry;
  GtkWidget *	addr2_entry;
  GtkWidget *	addr3_entry;
  GtkWidget *	addr4_entry;
  GtkWidget *	phone_entry;
  GtkWidget *	fax_entry;
  GtkWidget *	email_entry;

  GtkWidget *	shipname_entry;
  GtkWidget *	shipaddr1_entry;
  GtkWidget *	shipaddr2_entry;
  GtkWidget *	shipaddr3_entry;
  GtkWidget *	shipaddr4_entry;
  GtkWidget *	shipphone_entry;
  GtkWidget *	shipfax_entry;
  GtkWidget *	shipemail_entry;

  GtkWidget *	terms_amount;
  GtkWidget *	discount_amount;
  GtkWidget *	credit_amount;

  GtkWidget *	active_check;
  GtkWidget *	taxincluded_check;
  GtkWidget *	notes_text;

  CustomerDialogType	dialog_type;
  GUID		customer_guid;
  gint		component_id;
  GncBusiness *	business;
  GncCustomer *	created_customer;

} CustomerWindow;

static GncCustomer *
cw_get_customer (CustomerWindow *cw)
{
  if (!cw)
    return NULL;

  return gncBusinessLookupGUID (cw->business, GNC_CUSTOMER_MODULE_NAME,
				&cw->customer_guid);
}

static void gnc_ui_to_customer (CustomerWindow *cw, GncCustomer *cust)
{
  GncAddress *addr, *shipaddr;
  gnc_numeric num;

  addr = gncCustomerGetAddr (cust);
  shipaddr = gncCustomerGetShipAddr (cust);

  gnc_suspend_gui_refresh ();

  gncCustomerSetID (cust, gtk_editable_get_chars
		    (GTK_EDITABLE (cw->id_entry), 0, -1));
  gncCustomerSetName (cust, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->company_entry), 0, -1));

  gncAddressSetName (addr, gtk_editable_get_chars
		     (GTK_EDITABLE (cw->name_entry), 0, -1));
  gncAddressSetAddr1 (addr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->addr1_entry), 0, -1));
  gncAddressSetAddr2 (addr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->addr2_entry), 0, -1));
  gncAddressSetAddr3 (addr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->addr3_entry), 0, -1));
  gncAddressSetAddr4 (addr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->addr4_entry), 0, -1));
  gncAddressSetPhone (addr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->phone_entry), 0, -1));
  gncAddressSetFax (addr, gtk_editable_get_chars
		    (GTK_EDITABLE (cw->fax_entry), 0, -1));
  gncAddressSetEmail (addr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->email_entry), 0, -1));

  gncAddressSetName (shipaddr, gtk_editable_get_chars
		     (GTK_EDITABLE (cw->shipname_entry), 0, -1));
  gncAddressSetAddr1 (shipaddr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->shipaddr1_entry), 0, -1));
  gncAddressSetAddr2 (shipaddr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->shipaddr2_entry), 0, -1));
  gncAddressSetAddr3 (shipaddr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->shipaddr3_entry), 0, -1));
  gncAddressSetAddr4 (shipaddr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->shipaddr4_entry), 0, -1));
  gncAddressSetPhone (shipaddr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->shipphone_entry), 0, -1));
  gncAddressSetFax (shipaddr, gtk_editable_get_chars
		    (GTK_EDITABLE (cw->shipfax_entry), 0, -1));
  gncAddressSetEmail (shipaddr, gtk_editable_get_chars
		      (GTK_EDITABLE (cw->shipemail_entry), 0, -1));

  gncCustomerSetActive (cust, gtk_toggle_button_get_active
			(GTK_TOGGLE_BUTTON (cw->active_check)));
  gncCustomerSetTaxIncluded (cust, gtk_toggle_button_get_active
			     (GTK_TOGGLE_BUTTON (cw->taxincluded_check)));
  gncCustomerSetNotes (cust, gtk_editable_get_chars
		       (GTK_EDITABLE (cw->notes_text), 0, -1));

  /* Parse and set the terms, discount, and credit amounts */
  num = gnc_amount_edit_get_amount (GNC_AMOUNT_EDIT (cw->terms_amount));
  gncCustomerSetTerms (cust, gnc_numeric_num (num));
  gncCustomerSetDiscount (cust, gnc_amount_edit_get_amount
			  (GNC_AMOUNT_EDIT (cw->discount_amount)));
  gncCustomerSetCredit (cust, gnc_amount_edit_get_amount
			(GNC_AMOUNT_EDIT (cw->credit_amount)));

  gncCustomerCommitEdit (cust);
  gnc_resume_gui_refresh ();
}

static gboolean check_edit_amount (GtkWidget *dialog, GtkWidget *amount,
				   gnc_numeric *min, gnc_numeric *max,
				   const char * error_message)
{
  if (!gnc_amount_edit_evaluate (GNC_AMOUNT_EDIT (amount))) {
    if (error_message)
      gnc_error_dialog_parented (GTK_WINDOW (dialog), error_message);
    return TRUE;
  }
  /* We've got a valid-looking number; check mix/max */
  if (min || max) {
    gnc_numeric val = gnc_amount_edit_get_amount (GNC_AMOUNT_EDIT (amount));
    if ((min && gnc_numeric_compare (*min, val) > 0) ||
	(max && gnc_numeric_compare (val, *max) > 0)) {
      if (error_message)
	gnc_error_dialog_parented (GTK_WINDOW (dialog), error_message);
      return TRUE;
    }
  }
  return FALSE;
}

static gboolean check_entry_nonempty (GtkWidget *dialog, GtkWidget *entry, 
				      const char * error_message)
{
  const char *res = gtk_entry_get_text (GTK_ENTRY (entry));
  if (safe_strcmp (res, "") == 0) {
    if (error_message)
      gnc_error_dialog_parented (GTK_WINDOW (dialog), error_message);
    return TRUE;
  }
  return FALSE;
}

static void
gnc_customer_window_ok_cb (GtkWidget *widget, gpointer data)
{
  CustomerWindow *cw = data;
  char *res;
  GncCustomer *cust;
  gnc_numeric min, max;

  /* Check for valid id */
  if (check_entry_nonempty (cw->dialog, cw->id_entry,
			    _("The Customer must be given an ID.")))
    return;

  /* Check for valid company name */
  if (check_entry_nonempty (cw->dialog, cw->company_entry,
		   _("You must enter a company name.")))
    return;

  /* Make sure we have an address */
  if (check_entry_nonempty (cw->dialog, cw->addr1_entry, NULL) &&
      check_entry_nonempty (cw->dialog, cw->addr2_entry, NULL) &&
      check_entry_nonempty (cw->dialog, cw->addr3_entry, NULL) &&
      check_entry_nonempty (cw->dialog, cw->addr4_entry, NULL)) {
    const char *msg = _("You must enter a billing address.");
    gnc_error_dialog_parented (GTK_WINDOW (cw->dialog), msg);
    return;
  }

  /* Verify terms, discount, and credit are valid (or empty) */
  min = gnc_numeric_zero ();
  max = gnc_numeric_create (100, 1);
  if (check_edit_amount (cw->dialog, cw->terms_amount, &min, NULL,
			 _("Terms must be a positive integer or "
			   "you must leave it blank.")))
    return;

  if (check_edit_amount (cw->dialog, cw->discount_amount, &min, &max,
			 _("Discount percentage must be between 0-100 "
			   "or you must leave it blank.")))
    return;

  if (check_edit_amount (cw->dialog, cw->credit_amount, &min, NULL,
			 _("Credit must be a positive amount or "
			   "you must leave it blank.")))
    return;

  /* Now save it off */
  {
    GncCustomer *customer = cw_get_customer (cw);
    if (customer) {
      gnc_ui_to_customer (cw, customer);
    }
    cw->created_customer = customer;
    cw->customer_guid = *xaccGUIDNULL ();
  }

  gnc_close_gui_component (cw->component_id);
}

static void
gnc_customer_window_cancel_cb (GtkWidget *widget, gpointer data)
{
  CustomerWindow *cw = data;

  gnc_close_gui_component (cw->component_id);
}

static void
gnc_customer_window_help_cb (GtkWidget *widget, gpointer data)
{
  CustomerWindow *cw = data;
  char *help_file = "";		/* xxx */

  helpWindow(NULL, NULL, help_file);
}

static void
gnc_customer_window_destroy_cb (GtkWidget *widget, gpointer data)
{
  CustomerWindow *cw = data;
  GncCustomer *customer = cw_get_customer (cw);

  gnc_suspend_gui_refresh ();

  if (cw->dialog_type == NEW_CUSTOMER && customer != NULL) {
    gncCustomerDestroy (customer);
    cw->customer_guid = *xaccGUIDNULL ();
  }

  gnc_unregister_gui_component (cw->component_id);
  gnc_resume_gui_refresh ();

  g_free (cw);
}

static void
gnc_customer_name_changed_cb (GtkWidget *widget, gpointer data)
{
  CustomerWindow *cw = data;
  char *name, *id, *fullname, *title;

  if (!cw)
    return;

  name = gtk_entry_get_text (GTK_ENTRY (cw->name_entry));
  if (!name || *name == '\0')
    name = _("<No name>");

  id = gtk_entry_get_text (GTK_ENTRY (cw->id_entry));

  fullname = g_strconcat (name, " (", id, ")", NULL);

  if (cw->dialog_type == EDIT_CUSTOMER)
    title = g_strconcat (_("Edit Customer"), " - ", fullname, NULL);
  else
    title = g_strconcat (_("New Customer"), " - ", fullname, NULL);

  gtk_window_set_title (GTK_WINDOW (cw->dialog), title);

  g_free (fullname);
  g_free (title);
}

static int
gnc_customer_on_close_cb (GnomeDialog *dialog, gpointer data)
{
  CustomerWindow *cw;
  GncCustomer **created_customer = data;

  if (data) {
    cw = gtk_object_get_data (GTK_OBJECT (dialog), "dialog_info");
    *created_customer = cw->created_customer;
  }

  gtk_main_quit ();

  return FALSE;
}

static void
gnc_customer_window_close_handler (gpointer user_data)
{
  CustomerWindow *cw = user_data;

  gnome_dialog_close (GNOME_DIALOG (cw->dialog));
}

static void
gnc_customer_window_refresh_handler (GHashTable *changes, gpointer user_data)
{
  CustomerWindow *cw = user_data;
  const EventInfo *info;
  GncCustomer *customer = cw_get_customer (cw);

  /* If there isn't a customer behind us, close down */
  if (!customer) {
    gnc_close_gui_component (cw->component_id);
    return;
  }

  /* Next, close if this is a destroy event */
  if (changes) {
    info = gnc_gui_get_entity_events (changes, &cw->customer_guid);
    if (info && (info->event_mask & GNC_EVENT_DESTROY)) {
      gnc_close_gui_component (cw->component_id);
      return;
    }
  }
}

static CustomerWindow *
gnc_customer_new_window (GtkWidget *parent, GncBusiness *bus,
			 GncCustomer *cust)
{
  CustomerWindow *cw;
  GladeXML *xml;
  GtkWidget *hbox, *edit;
  GnomeDialog *cwd;
  gnc_commodity *commodity;
  GNCPrintAmountInfo print_info;

  cw = g_new0 (CustomerWindow, 1);

  cw->business = bus;

  /* Find the dialog */
  xml = gnc_glade_xml_new ("customer.glade", "Customer Dialog");
  cw->dialog = glade_xml_get_widget (xml, "Customer Dialog");
  cwd = GNOME_DIALOG (cw->dialog);

  gtk_object_set_data (GTK_OBJECT (cw->dialog), "dialog_info", cw);

  /* default to ok */
  gnome_dialog_set_default (cwd, 0);

  if (parent)
    gnome_dialog_set_parent (cwd, GTK_WINDOW (parent));

  /* Get entry points */
  cw->id_entry = glade_xml_get_widget (xml, "id_entry");
  cw->company_entry = glade_xml_get_widget (xml, "company_entry");

  cw->name_entry = glade_xml_get_widget (xml, "name_entry");
  cw->addr1_entry = glade_xml_get_widget (xml, "addr1_entry");
  cw->addr2_entry = glade_xml_get_widget (xml, "addr2_entry");
  cw->addr3_entry = glade_xml_get_widget (xml, "addr3_entry");
  cw->addr4_entry = glade_xml_get_widget (xml, "addr4_entry");
  cw->phone_entry = glade_xml_get_widget (xml, "phone_entry");
  cw->fax_entry = glade_xml_get_widget (xml, "fax_entry");
  cw->email_entry = glade_xml_get_widget (xml, "email_entry");

  cw->shipname_entry = glade_xml_get_widget (xml, "shipname_entry");
  cw->shipaddr1_entry = glade_xml_get_widget (xml, "shipaddr1_entry");
  cw->shipaddr2_entry = glade_xml_get_widget (xml, "shipaddr2_entry");
  cw->shipaddr3_entry = glade_xml_get_widget (xml, "shipaddr3_entry");
  cw->shipaddr4_entry = glade_xml_get_widget (xml, "shipaddr4_entry");
  cw->shipphone_entry = glade_xml_get_widget (xml, "shipphone_entry");
  cw->shipfax_entry = glade_xml_get_widget (xml, "shipfax_entry");
  cw->shipemail_entry = glade_xml_get_widget (xml, "shipemail_entry");

  cw->active_check = glade_xml_get_widget (xml, "active_check");
  cw->taxincluded_check = glade_xml_get_widget (xml, "tax_included_check");
  cw->notes_text = glade_xml_get_widget (xml, "notes_text");

  /* TERMS: Integer Value */
  edit = gnc_amount_edit_new();
  gnc_amount_edit_set_evaluate_on_enter (GNC_AMOUNT_EDIT (edit), TRUE);
  print_info = gnc_integral_print_info ();
  gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (edit), print_info);
  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (edit), 1);
  cw->terms_amount = edit;
  gtk_widget_show (edit);

  hbox = glade_xml_get_widget (xml, "terms_box");
  gtk_box_pack_start (GTK_BOX (hbox), edit, TRUE, TRUE, 0);

  /* DISCOUNT: Percentage Value */
  edit = gnc_amount_edit_new();
  gnc_amount_edit_set_evaluate_on_enter (GNC_AMOUNT_EDIT (edit), TRUE);
  print_info.max_decimal_places = 5;
  gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (edit), print_info);
  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (edit), 100000);
  cw->discount_amount = edit;
  gtk_widget_show (edit);

  hbox = glade_xml_get_widget (xml, "discount_box");
  gtk_box_pack_start (GTK_BOX (hbox), edit, TRUE, TRUE, 0);

  /* CREDIT: Monetary Value */
  edit = gnc_amount_edit_new();
  commodity = gnc_default_currency ();
  print_info = gnc_commodity_print_info (commodity, FALSE);
  gnc_amount_edit_set_evaluate_on_enter (GNC_AMOUNT_EDIT (edit), TRUE);
  gnc_amount_edit_set_print_info (GNC_AMOUNT_EDIT (edit), print_info);
  gnc_amount_edit_set_fraction (GNC_AMOUNT_EDIT (edit),
                                gnc_commodity_get_fraction (commodity));
  cw->credit_amount = edit;
  gtk_widget_show (edit);

  hbox = glade_xml_get_widget (xml, "credit_box");
  gtk_box_pack_start (GTK_BOX (hbox), edit, TRUE, TRUE, 0);

  /* Setup Dialog for Editing */
  gnome_dialog_set_default (cwd, 0);

  /* Attach <Enter> to default button */
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->id_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->company_entry));

  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->name_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->addr1_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->addr2_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->addr3_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->addr4_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->phone_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->fax_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->email_entry));

  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipname_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipaddr1_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipaddr2_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipaddr3_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipaddr4_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipphone_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipfax_entry));
  gnome_dialog_editable_enters (cwd, GTK_EDITABLE (cw->shipemail_entry));

  /* Set focus to company name */
  gtk_widget_grab_focus (cw->company_entry);

  /* Setup signals */
  gnome_dialog_button_connect
    (cwd, 0, GTK_SIGNAL_FUNC(gnc_customer_window_ok_cb), cw);
  gnome_dialog_button_connect
    (cwd, 1, GTK_SIGNAL_FUNC(gnc_customer_window_cancel_cb), cw);
  gnome_dialog_button_connect
    (cwd, 2, GTK_SIGNAL_FUNC(gnc_customer_window_help_cb), cw);

  gtk_signal_connect (GTK_OBJECT (cw->dialog), "destroy",
		      GTK_SIGNAL_FUNC(gnc_customer_window_destroy_cb), cw);

  gtk_signal_connect(GTK_OBJECT (cw->id_entry), "changed",
		     GTK_SIGNAL_FUNC(gnc_customer_name_changed_cb), cw);

  gtk_signal_connect(GTK_OBJECT (cw->company_entry), "changed",
		     GTK_SIGNAL_FUNC(gnc_customer_name_changed_cb), cw);

  /* Setup initial values */
  if (cust != NULL) {
    GncAddress *addr, *shipaddr;
    const char *string;
    gint pos = 0;

    cw->dialog_type = EDIT_CUSTOMER;
    cw->customer_guid = *gncCustomerGetGUID (cust);

    addr = gncCustomerGetAddr (cust);
    shipaddr = gncCustomerGetShipAddr (cust);

    gtk_entry_set_text (GTK_ENTRY (cw->id_entry), gncCustomerGetID (cust));
    gtk_entry_set_editable (GTK_ENTRY (cw->id_entry), FALSE);

    gtk_entry_set_text (GTK_ENTRY (cw->company_entry), gncCustomerGetName (cust));

    /* Setup Address */
    gtk_entry_set_text (GTK_ENTRY (cw->name_entry), gncAddressGetName (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->addr1_entry), gncAddressGetAddr1 (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->addr2_entry), gncAddressGetAddr2 (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->addr3_entry), gncAddressGetAddr3 (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->addr4_entry), gncAddressGetAddr4 (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->phone_entry), gncAddressGetPhone (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->fax_entry), gncAddressGetFax (addr));
    gtk_entry_set_text (GTK_ENTRY (cw->email_entry), gncAddressGetEmail (addr));

    /* Setup Ship-to Address */
    gtk_entry_set_text (GTK_ENTRY (cw->shipname_entry), gncAddressGetName (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipaddr1_entry), gncAddressGetAddr1 (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipaddr2_entry), gncAddressGetAddr2 (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipaddr3_entry), gncAddressGetAddr3 (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipaddr4_entry), gncAddressGetAddr4 (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipphone_entry), gncAddressGetPhone (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipfax_entry), gncAddressGetFax (shipaddr));
    gtk_entry_set_text (GTK_ENTRY (cw->shipemail_entry), gncAddressGetEmail (shipaddr));

    /* Set toggle buttons */
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw->active_check),
                                gncCustomerGetActive (cust));

    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (cw->taxincluded_check),
				  gncCustomerGetTaxIncluded (cust));

    string = gncCustomerGetNotes (cust);
    gtk_editable_delete_text (GTK_EDITABLE (cw->notes_text), 0, -1);
    gtk_editable_insert_text (GTK_EDITABLE (cw->notes_text), string,
			      strlen(string), &pos);

    cw->component_id =
      gnc_register_gui_component (DIALOG_EDIT_CUSTOMER_CM_CLASS,
				  gnc_customer_window_refresh_handler,
				  gnc_customer_window_close_handler,
				  cw);
  } else {
    gnc_numeric num;
    cust = gncCustomerCreate (bus);
    cw->customer_guid = *gncCustomerGetGUID (cust);

    cw->dialog_type = NEW_CUSTOMER;
    gtk_entry_set_text (GTK_ENTRY (cw->id_entry),
			g_strdup_printf ("%.6d", gncCustomerNextID(bus)));
    cw->component_id =
      gnc_register_gui_component (DIALOG_NEW_CUSTOMER_CM_CLASS,
				  gnc_customer_window_refresh_handler,
				  gnc_customer_window_close_handler,
				  cw);
  }


  /* I know that cust exists here -- either passed in or just created */
  {
    gnc_numeric terms;

    /* Set the Terms, Discount, and Credit amounts */
    terms = gnc_numeric_create (gncCustomerGetTerms (cust), 1);
    gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (cw->terms_amount), terms);
    gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (cw->discount_amount),
				gncCustomerGetDiscount (cust));
    gnc_amount_edit_set_amount (GNC_AMOUNT_EDIT (cw->credit_amount),
				gncCustomerGetCredit (cust));
  }

  gnc_gui_component_watch_entity_type (cw->component_id,
				       GNC_ID_NONE,
				       GNC_EVENT_MODIFY | GNC_EVENT_DESTROY);

  gtk_widget_show_all (cw->dialog);

  return cw;
}

GncCustomer *
gnc_customer_new (GtkWidget *parent, GncBusiness *bus)
{
  CustomerWindow *cw;
  GncCustomer *created_customer = NULL;

  /* Make sure required options exist */
  if (!bus) return NULL;

  cw = gnc_customer_new_window (parent, bus, NULL);

  gtk_signal_connect (GTK_OBJECT (cw->dialog), "close",
		      GTK_SIGNAL_FUNC (gnc_customer_on_close_cb),
		      &created_customer);

  gtk_window_set_modal (GTK_WINDOW (cw->dialog), TRUE);

  gtk_main ();

  return created_customer;
}

void
gnc_customer_edit (GtkWidget *parent, GncCustomer *cust)
{
  CustomerWindow *cw;

  if (!cust) return;

  cw = gnc_customer_new_window (parent, gncCustomerGetBusiness(cust), cust);

  gtk_signal_connect (GTK_OBJECT (cw->dialog), "close",
		      GTK_SIGNAL_FUNC (gnc_customer_on_close_cb),
		      NULL);

  gtk_window_set_modal (GTK_WINDOW (cw->dialog), TRUE);

  gtk_main ();

  return;
}

/* Functions for widgets for customer selection */

static gpointer gnc_customer_edit_new_cb (gpointer arg, GtkWidget *toplevel)
{
  struct _customer_select_window *sw = arg;

  if (!arg) return NULL;

  return gnc_customer_new (toplevel, sw->business);
}

static void gnc_customer_edit_edit_cb (gpointer arg, gpointer obj, GtkWidget *toplevel)
{
  GncCustomer *cust = obj;
  struct _customer_select_window *sw = arg;

  if (!arg || !obj) return;

  gnc_customer_edit (toplevel, cust);
}

gpointer gnc_customer_edit_new_select (gpointer bus, gpointer cust,
				       GtkWidget *toplevel)
{
  GncBusiness *business = bus;
  struct _customer_select_window sw;

  g_return_val_if_fail (bus != NULL, NULL);

  sw.business = business;

  return
    gnc_ui_business_chooser_new (toplevel, cust,
				 business, GNC_CUSTOMER_MODULE_NAME,
				 gnc_customer_edit_new_cb,
				 gnc_customer_edit_edit_cb, &sw);
}

gpointer gnc_customer_edit_new_edit (gpointer bus, gpointer cust,
				     GtkWidget *toplevel)
{
  GncBusiness *busiess = bus;
  GncCustomer *customer = cust;

  g_return_val_if_fail (cust != NULL, NULL);

  gnc_customer_edit (toplevel, cust);
  return cust;
}