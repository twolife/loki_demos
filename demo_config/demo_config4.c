
/* Load and save the preferences */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <fcntl.h>
#include <unistd.h>

#include <gtk/gtk.h>

static const char *product;
static char *title;
GtkWidget *main_window;

static struct prefentry {
    enum {
        BOOL_PREF,
        SEPARATOR_PREF,
        LABEL_PREF,
        RADIO_PREF,
        FILE_PREF
    } type;

    union {
        struct {
            char *label;
        } label_prefs;

        struct {
            char *label;
            char *true_option;
            char *false_option;
            gboolean value;
        } bool_prefs;

        struct {
            char *label;
            gboolean enabled;
            struct radio_option {
                char *label;
                char *value;
                gboolean enabled;
                GtkWidget *button;
                struct radio_option *next;
            } *options, *tail_option;
        } radio_prefs;

        struct {
            char *label;
            char *format;
            char *value;
            gboolean enabled;
            GtkWidget *toggle;
            GtkWidget *entry;
            GtkWidget *button;
        } file_prefs;
    } data;

    struct prefentry *next;
} *prefs = NULL, *last_entry = NULL;


/* Parse a command line buffer into arguments */
static int parse_line(char *line, char **argv)
{
	char *bufp;
	int argc;

	argc = 0;
	for ( bufp = line; *bufp; ) {
		/* Skip leading whitespace */
		while ( isspace(*bufp) ) {
			++bufp;
		}
		/* Skip over argument */
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && (*bufp != '"') ) {
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

static void add_label_option(char **args)
{
    struct prefentry *entry;

    entry = (struct prefentry *)malloc(sizeof *entry);
    if ( entry ) {
        /* Fill the data */
        entry->type = LABEL_PREF;
        entry->data.label_prefs.label = strdup(args[1]);
        entry->next = NULL;

        /* Add it to our list */
        if ( last_entry ) {
            last_entry->next = entry;
        } else {
            prefs = entry;
        }
        last_entry = entry;
    }
}

static void add_separator_option(char **args)
{
    struct prefentry *entry;

    entry = (struct prefentry *)malloc(sizeof *entry);
    if ( entry ) {
        /* Fill the data */
        entry->type = SEPARATOR_PREF;
        entry->next = NULL;

        /* Add it to our list */
        if ( last_entry ) {
            last_entry->next = entry;
        } else {
            prefs = entry;
        }
        last_entry = entry;
    }
}

static void add_bool_option(char **args)
{
    struct prefentry *entry;

    entry = (struct prefentry *)malloc(sizeof *entry);
    if ( entry ) {
        /* Fill the data */
        entry->type = BOOL_PREF;
        entry->data.bool_prefs.label = strdup(args[1]);
        entry->data.bool_prefs.true_option = strdup(args[2]);
        entry->data.bool_prefs.false_option = strdup(args[3]);
        if ( strcasecmp(args[4], "true") == 0 ) {
            entry->data.bool_prefs.value = TRUE;
        } else {
            entry->data.bool_prefs.value = FALSE;
        }
        entry->next = NULL;

        /* Add it to our list */
        if ( last_entry ) {
            last_entry->next = entry;
        } else {
            prefs = entry;
        }
        last_entry = entry;
    }
}

static void add_radio_option(char **args)
{
    struct prefentry *entry;
    int i;

    entry = (struct prefentry *)malloc(sizeof *entry);
    if ( entry ) {
        /* Fill the data */
        entry->type = RADIO_PREF;
        entry->data.radio_prefs.label = strdup(args[1]);
        if ( strcasecmp(args[2], "always") == 0 ) {
            entry->data.radio_prefs.enabled = TRUE*2;
        } else
        if ( strcasecmp(args[2], "true") == 0 ) {
            entry->data.radio_prefs.enabled = TRUE;
        } else {
            entry->data.radio_prefs.enabled = FALSE;
        }
        entry->data.radio_prefs.options = NULL;
        entry->data.radio_prefs.tail_option = NULL;
        for ( i=3; args[i] && args[i+1] && args[i+2] && args[i+3]; i += 4 ) {
            struct radio_option *option;

            if ( strcasecmp(args[i], "OPTION") != 0 ) {
                /* Urkh, syntax error.. how to report it? */
                continue;
            }
            option = (struct radio_option *)malloc(sizeof *option);
            if ( option ) {
                option->label = strdup(args[i+1]);
                option->value = strdup(args[i+2]);
                if ( strcasecmp(args[i+3], "true") == 0 ) {
                    option->enabled = TRUE;
                } else {
                    option->enabled = FALSE;
                }
                option->next = NULL;
                if ( entry->data.radio_prefs.tail_option ) {
                    entry->data.radio_prefs.tail_option->next = option;
                } else {
                    entry->data.radio_prefs.options = option;
                }
                entry->data.radio_prefs.tail_option = option;
            }
        }
        entry->next = NULL;

        /* Add it to our list */
        if ( last_entry ) {
            last_entry->next = entry;
        } else {
            prefs = entry;
        }
        last_entry = entry;
    }
}

static void add_file_option(char **args)
{
    struct prefentry *entry;

    entry = (struct prefentry *)malloc(sizeof *entry);
    if ( entry ) {
        /* Fill the data */
        entry->type = FILE_PREF;
        entry->data.file_prefs.label = strdup(args[1]);
        entry->data.file_prefs.format = strdup(args[2]);
        entry->data.file_prefs.value = strdup(args[3]);
        if ( strcasecmp(args[4], "true") == 0 ) {
            entry->data.file_prefs.enabled = TRUE;
        } else {
            entry->data.file_prefs.enabled = FALSE;
        }
        entry->next = NULL;

        /* Add it to our list */
        if ( last_entry ) {
            last_entry->next = entry;
        } else {
            prefs = entry;
        }
        last_entry = entry;
    }
}

int load_prefs(const char *product)
{
    char path[PATH_MAX];
    FILE *fp;
    int lineno;
    char line[1024];
    int length;
    int nargs;
    char **args;

    /* Open the existing preferences file */
    sprintf(path, "%s/.loki/loki_demos/%s/prefs.txt", getenv("HOME"), product);
    fp = fopen(path, "r");
    if ( ! fp ) {
        sprintf(path, "./demos/%s/launch/prefs.txt", product);
        fp = fopen(path, "r");
    }
    if ( ! fp ) {
        return(-1);
    }

    /* Load the title line */
    if ( ! fgets(line, sizeof(line), fp) ) {
        fclose(fp);
        unlink(path);
        return(-1);
    }
    line[strlen(line)-1] = '\0';
    title = strdup(line);

    /* Load the rest of the options */
    lineno = 0;
    line[0] = 0;
    length = 0;
    while ( fgets(&line[length], sizeof(line)-length, fp) ) {
        line[strlen(line)-1] = '\0';
        ++lineno;
        if ( line[strlen(line)-1] == '\\' ) {
            length = strlen(line)-1;
            continue;
        }

	    /* Parse it into arguments */
	    nargs = parse_line(line, NULL);
        if ( nargs < 1 ) {
            continue;
        }
	    args = (char **)malloc((nargs+1)*(sizeof *args));
	    if ( args == NULL ) {
            continue;
	    }
	    parse_line(line, args);

        if ( strcasecmp(args[0], "LABEL") == 0 ) {
            if ( nargs == 2 ) {
                add_label_option(args);
            } else {
                fprintf(stderr, "Line %d: LABEL requires an argument\n",lineno);
            }
        } else
        if ( strcasecmp(args[0], "SEPARATOR") == 0 ) {
            add_separator_option(args);
        } else
        if ( strcasecmp(args[0], "BOOL") == 0 ) {
            if ( nargs == 5 ) {
                add_bool_option(args);
            } else {
                fprintf(stderr, "Line %d: BOOL requires 4 arguments\n", lineno);
            }
        } else
        if ( strcasecmp(args[0], "RADIO") == 0 ) {
            if ( nargs >= 7 ) {
                add_radio_option(args);
            } else {
                fprintf(stderr, "Line %d: RADIO requires more arguments\n", lineno);
            }
        } else
        if ( strcasecmp(args[0], "FILE") == 0 ) {
            if ( nargs == 5 ) {
                add_file_option(args);
            } else {
                fprintf(stderr, "Line %d: FILE requires 4 arguments\n", lineno);
            }
        } else {
            fprintf(stderr, "Line %d: Unknown keyword\n", lineno);
        }
        free(args);

        length = 0;
    }
    fclose(fp);
    return(0);
}

int save_prefs(const char *product)
{
    char path[PATH_MAX];
    FILE *fp;
    struct prefentry *entry;
    char command[2*PATH_MAX];

    /* Save the preferences */
    sprintf(path, "%s/.loki", getenv("HOME"));
    mkdir(path, 0700);
    strcat(path, "/loki_demos");
    mkdir(path, 0700);
    strcat(path, "/");
    strcat(path, product);
    mkdir(path, 0700);
    strcat(path, "/prefs.txt");
    fp = fopen(path, "w");
    if ( ! fp ) {
        fprintf(stderr, "Unable to write to %s\n", path);
        return(-1);
    }
    fprintf(fp, "%s\n", title);
    
    for ( entry = prefs; entry; entry = entry->next ) {
        switch(entry->type) {
            case LABEL_PREF:
                fprintf(fp, "LABEL \"%s\"\n", entry->data.bool_prefs.label);
                break;
            case SEPARATOR_PREF:
                fprintf(fp, "SEPARATOR\n");
                break;
            case BOOL_PREF:
                fprintf(fp, "BOOL \"%s\" \"%s\" \"%s\" %s\n",
                    entry->data.bool_prefs.label,
                    entry->data.bool_prefs.true_option,
                    entry->data.bool_prefs.false_option,
                    entry->data.bool_prefs.value ? "TRUE" : "FALSE");
                break;
            case RADIO_PREF:
                fprintf(fp, "RADIO \"%s\" %s \\\n",
                    entry->data.radio_prefs.label,
                    entry->data.radio_prefs.enabled ? 
                    (entry->data.radio_prefs.enabled == TRUE*2 ? 
                        "ALWAYS" : "TRUE") : "FALSE");
                { struct radio_option *option;
                    for ( option = entry->data.radio_prefs.options;
                          option;
                          option = option->next ) {
                        fprintf(fp, "OPTION \"%s\" \"%s\" %s",
                            option->label, option->value,
                            option->enabled ? "TRUE" : "FALSE");
                        if ( option->next ) {
                            fprintf(fp, " \\\n");
                        } else {
                            fprintf(fp, "\n");
                        }
                    }
                }
                break;
            case FILE_PREF:
                free(entry->data.file_prefs.value);
                entry->data.file_prefs.value = (char *) gtk_editable_get_text(
                        GTK_EDITABLE(entry->data.file_prefs.entry));
                fprintf(fp, "FILE \"%s\" \"%s\" \"%s\" %s\n",
                    entry->data.file_prefs.label,
                    entry->data.file_prefs.format,
                    entry->data.file_prefs.value,
                    entry->data.file_prefs.enabled ? "TRUE" : "FALSE");
                break;
        }
    }
    fclose(fp);

    /* Create a command line from them */
    command[0] = '\0';
    sprintf(path, "demos/%s/launch/launch.txt", product);
    fp = fopen(path, "r");
    if ( fp ) {
        if ( fgets(command, sizeof(command), fp) ) {
            command[strlen(command)-1] = '\0';
        }
    }
    if ( command[0] ) {
        sprintf(path, "%s/.loki/loki_demos/%s/launch.txt",
                getenv("HOME"), product);
        fp = fopen(path, "w");
        if ( ! fp ) {
            fprintf(stderr, "Unable to write to %s\n", path);
            return(-1);
        }
        for ( entry = prefs; entry; entry = entry->next ) {
            switch(entry->type) {
                case LABEL_PREF:
                    break;
                case SEPARATOR_PREF:
                    break;
                case BOOL_PREF:
                    strcat(command, " ");
                    if ( entry->data.bool_prefs.value ) {
                        strcat(command, entry->data.bool_prefs.true_option);
                    } else {
                        strcat(command, entry->data.bool_prefs.false_option);
                    }
                    break;
                case RADIO_PREF:
                    if ( entry->data.radio_prefs.enabled ) {
                        struct radio_option *option;
                        for ( option = entry->data.radio_prefs.options;
                              option;
                              option = option->next ) {
                            if ( option->enabled ) {
                                strcat(command, " ");
                                strcat(command, option->value);
                            }
                        }
                    }
                    break;
                case FILE_PREF:
                    if ( entry->data.file_prefs.enabled ) {
                        strcat(command, " ");
                        sprintf(&command[strlen(command)],
                            entry->data.file_prefs.format,
                            entry->data.file_prefs.value);
                    }
                    break;
            }
        }
        fprintf(fp, "%s\n", command);
        fclose(fp);
    }
    return(0);
}

static void on_alert_close (GObject *source, GAsyncResult *res, gpointer user_data) {
    g_application_quit(G_APPLICATION(gtk_window_get_application(GTK_WINDOW (user_data))));
}

static void message(const char *text) {
    GtkAlertDialog *dialog = gtk_alert_dialog_new ("Error!");
    gtk_alert_dialog_set_detail (dialog, text);
    gtk_alert_dialog_choose (dialog, GTK_WINDOW (main_window), NULL, on_alert_close, main_window);
}

static gboolean load_file( GtkTextView *widget, const char *filename )
{
    gboolean status;
    g_autoptr (GFile) file = g_file_new_for_path (filename);
    g_autofree char *contents = NULL;
    gsize length = 0;
    g_autoptr (GError) error = NULL;
    status = g_file_load_contents(
                 file, NULL, &contents, &length,
                 NULL, &error);

    if (error != NULL) {
      g_printerr ("Unable to open “%s”: %s\n",
                  g_file_peek_path (file),
                  error->message);
    } else {
        GtkTextBuffer* buffer = gtk_text_view_get_buffer(widget);
        gtk_text_buffer_set_text (buffer, contents, length);
        GtkTextIter start;
        gtk_text_buffer_get_start_iter (buffer, &start);
        gtk_text_buffer_place_cursor (buffer, &start);
    }

    return status;
}

void close_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWindow *window = GTK_WINDOW(data);
    gtk_window_close(window);
    gtk_window_destroy(window);
}

void view_readme_slot( GtkWidget* w, gpointer data )
{
    GtkWidget *readme_window = gtk_window_new ();
    gtk_window_set_title (GTK_WINDOW (readme_window), "Readme File");
    gtk_window_set_default_size(GTK_WINDOW (readme_window), 680, 500);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    GtkWidget *scrolled = gtk_scrolled_window_new();
    gtk_box_append(GTK_BOX(vbox), scrolled);

    GtkWidget *text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text), false);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text), true);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text), GTK_WRAP_WORD);
    gtk_widget_set_vexpand(text, true);
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled), text);

    GtkWidget *close_button = gtk_button_new_with_label("Close");
    g_signal_connect (close_button, "clicked", G_CALLBACK (close_readme_slot), readme_window);
    gtk_box_append(GTK_BOX(vbox), close_button);

    gtk_window_set_child (GTK_WINDOW(readme_window), vbox);

    char *readme_file;
    const char *template = "demos/%s/README";
    size_t len = strlen(template) + strlen(product);
    readme_file = malloc(len);
    snprintf(readme_file, len, template, product);

    load_file(GTK_TEXT_VIEW(text), readme_file);
    free(readme_file);

    gtk_window_present (GTK_WINDOW(readme_window));
}

void file_okay_slot (GObject      *source,
                     GAsyncResult *result,
                     gpointer      user_data)
{
    struct prefentry *entry;
    GtkFileDialog *dialog = GTK_FILE_DIALOG (source);
    g_autoptr (GFile) file = gtk_file_dialog_open_finish (dialog, result, NULL);

    if (file != NULL) {
        /* Set the text entry with the filename from the dialog */
        entry = user_data;
        gtk_editable_set_text(GTK_EDITABLE(entry->data.file_prefs.entry),
            g_file_get_path(file));
    }
}

void file_button_slot(GtkWidget *w, gpointer data)
{
    GtkFileDialog *dialog = gtk_file_dialog_new();
    gtk_file_dialog_open (dialog, GTK_WINDOW(main_window), NULL, file_okay_slot, data);
}

static void bool_toggle_option( GtkWidget* widget, gpointer func_data)
{
    struct prefentry *entry = (struct prefentry *)func_data;

    if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)) ) {
        entry->data.bool_prefs.value = TRUE;
    } else {
        entry->data.bool_prefs.value = FALSE;
    }
}

static void radio_toggle_option( GtkWidget* widget, gpointer func_data)
{
    struct prefentry *entry = (struct prefentry *)func_data;
    struct radio_option *option;

    if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)) ) {
        entry->data.radio_prefs.enabled = TRUE;
    } else {
        entry->data.radio_prefs.enabled = FALSE;
    }
    for ( option = entry->data.radio_prefs.options;
          option;
          option = option->next ) {
        gtk_widget_set_sensitive(option->button, 
                                 entry->data.radio_prefs.enabled);
    }
}

static void radio_toggle_option_element( GtkWidget* widget, gpointer func_data)
{
    struct radio_option *option = (struct radio_option *)func_data;

    if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)) ) {
        option->enabled = TRUE;
    } else {
        option->enabled = FALSE;
    }
}

static void file_toggle_option( GtkWidget* widget, gpointer func_data)
{
    struct prefentry *entry = (struct prefentry *)func_data;

    if ( gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)) ) {
        entry->data.file_prefs.enabled = TRUE;
    } else {
        entry->data.file_prefs.enabled = FALSE;
    }
    gtk_widget_set_sensitive(entry->data.file_prefs.entry,
                             entry->data.file_prefs.enabled);
    gtk_widget_set_sensitive(entry->data.file_prefs.button,
                             entry->data.file_prefs.enabled);
}

void fill_pref_ui(GtkWidget *vbox)
{
    struct prefentry *entry;
    GtkWidget* widget;
    for ( entry = prefs; entry; entry = entry->next ) {
        switch(entry->type) {
            case LABEL_PREF:
                widget = gtk_label_new(entry->data.label_prefs.label);
                gtk_widget_set_halign(widget, GTK_ALIGN_START);
                gtk_box_append(GTK_BOX(vbox), widget);
                break;

            case SEPARATOR_PREF:
                widget = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
                gtk_box_append(GTK_BOX(vbox), widget);
                break;

            case BOOL_PREF:
                /* Create the check button itself */
                widget = gtk_check_button_new_with_label(
                            entry->data.bool_prefs.label);
                gtk_widget_set_halign(widget, GTK_ALIGN_START);
                gtk_check_button_set_active(GTK_CHECK_BUTTON(widget),
                            entry->data.bool_prefs.value);
                gtk_box_append(GTK_BOX(vbox), widget);

                g_signal_connect(widget, "toggled",
                    G_CALLBACK(bool_toggle_option), (gpointer)entry);

                break;

            case RADIO_PREF:
                bool disabled = false;

                /* Create the toggle button or label itself */
                if ( entry->data.radio_prefs.enabled == TRUE*2 ) {
                    widget = gtk_label_new(entry->data.radio_prefs.label);
                    gtk_widget_set_halign(widget, GTK_ALIGN_START);
                    gtk_box_append(GTK_BOX(vbox), widget);
                } else {
                    widget = gtk_check_button_new_with_label(
                                entry->data.radio_prefs.label);
                    gtk_widget_set_halign(widget, GTK_ALIGN_START);
                    gtk_box_append(GTK_BOX(vbox), widget);
                    gtk_check_button_set_active(GTK_CHECK_BUTTON(widget),
                                entry->data.radio_prefs.enabled);
                    disabled = !entry->data.radio_prefs.enabled;

                    g_signal_connect(widget, "toggled",
                        G_CALLBACK(radio_toggle_option), (gpointer)entry);
                }

                { struct radio_option *option;
                  GtkCheckButton *radio_group = NULL;
                    for ( option = entry->data.radio_prefs.options;
                          option;
                          option = option->next ) {
                        /* Create an hbox for this line */
                        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
                        gtk_box_append(GTK_BOX(vbox), hbox);

                        /* Add a spacing label */
                        widget = gtk_label_new("  ");
                        gtk_box_append(GTK_BOX(hbox), widget);

                        /* Add the radio button itself */
                        widget = gtk_check_button_new_with_label(option->label);
                        gtk_box_append(GTK_BOX(hbox), widget);
                        gtk_check_button_set_active(GTK_CHECK_BUTTON(widget), option->enabled);
                        if(radio_group == NULL) {
                            radio_group = GTK_CHECK_BUTTON(widget);
                        } else {
                            gtk_check_button_set_group(GTK_CHECK_BUTTON(widget), radio_group);
                        }
                        if(disabled) {
                            gtk_widget_set_sensitive(widget, false);
                        }
                        g_signal_connect(widget, "toggled",
                            G_CALLBACK(radio_toggle_option_element), (gpointer)option);
                        option->button = widget;
                    }
                }
                break;

            case FILE_PREF:
                /* Create an hbox for all the new widgets */
                GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
                gtk_box_append(GTK_BOX(vbox), hbox);

                /* Create the toggle button */
                widget = gtk_check_button_new_with_label(
                            entry->data.file_prefs.label);
                gtk_box_append(GTK_BOX(hbox), widget);
                gtk_check_button_set_active(GTK_CHECK_BUTTON(widget),
                            entry->data.file_prefs.enabled);
                g_signal_connect(widget, "toggled",
                    G_CALLBACK(file_toggle_option), (gpointer)entry);

                /* Create the value entry */
                widget = gtk_entry_new();
                gtk_box_append(GTK_BOX(hbox), widget);
                gtk_editable_set_text(GTK_EDITABLE(widget),
                                   entry->data.file_prefs.value);
                gtk_widget_set_sensitive(widget,
                                         entry->data.file_prefs.enabled);
                entry->data.file_prefs.entry = widget;

                /* Create the file dialog button */
                widget = gtk_button_new_with_label("File...");
                gtk_box_append(GTK_BOX(hbox), widget);
                gtk_widget_set_sensitive(widget,
                                         entry->data.file_prefs.enabled);
                g_signal_connect(widget, "clicked",
                    G_CALLBACK(file_button_slot), (gpointer)entry);
                entry->data.file_prefs.button = widget;
                break;
        }
    }
}

void cancel_button_slot(GtkWidget *w, gpointer data)
{
    g_application_quit(G_APPLICATION(gtk_window_get_application(GTK_WINDOW (main_window))));
}

void save_button_slot(GtkWidget *w, gpointer data)
{
    if ( save_prefs(product) < 0 ) {
        { char msg[128];
            sprintf(msg, "Unable to save preferences for %s", product);
            message(msg);
        }
    } else {
        g_application_quit(G_APPLICATION(gtk_window_get_application(GTK_WINDOW (main_window))));
    }
}

void app_activate (GApplication *app, gpointer *user_data) {
    main_window = gtk_window_new ();
    gtk_window_set_application (GTK_WINDOW (main_window), GTK_APPLICATION (app));
    gtk_window_set_title (GTK_WINDOW (main_window), "Loki Demo Preferences");

    if ( load_prefs(product) < 0 ) {
        { char msg[128];
            sprintf(msg, "Unable to load preferences for %s", product);
            message(msg);
        }
        return;
    }

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GdkPixbuf *pb = gdk_pixbuf_new_from_file("demo_config.xpm", NULL);
    GtkWidget *image = gtk_picture_new_for_pixbuf(pb);
    gtk_box_append(GTK_BOX(hbox), image);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(hbox), vbox);

    char msg[128];
    sprintf(msg, "<big>%s</big>", title);
    GtkWidget *product_label = gtk_label_new(msg);
    gtk_label_set_use_markup(GTK_LABEL(product_label), TRUE);
    gtk_box_append(GTK_BOX(vbox), product_label);

    GtkWidget *vbox_opts = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_box_append(GTK_BOX(vbox), vbox_opts);

    GtkWidget *cbox = gtk_center_box_new();
    gtk_box_append(GTK_BOX(vbox), cbox);
    GtkWidget *b1 = gtk_button_new_with_label("Cancel");
    gtk_center_box_set_start_widget(GTK_CENTER_BOX(cbox), b1);
    g_signal_connect (b1, "clicked", G_CALLBACK (cancel_button_slot), NULL);
    GtkWidget *b2 = gtk_button_new_with_label("README");
    gtk_center_box_set_center_widget(GTK_CENTER_BOX(cbox), b2);
    g_signal_connect (b2, "clicked", G_CALLBACK (view_readme_slot), NULL);
    GtkWidget *b3 = gtk_button_new_with_label("Save");
    gtk_center_box_set_end_widget(GTK_CENTER_BOX(cbox), b3);
    g_signal_connect (b3, "clicked", G_CALLBACK (save_button_slot), NULL);

    gtk_window_set_resizable (GTK_WINDOW(main_window), FALSE);
    gtk_window_set_child (GTK_WINDOW(main_window), hbox);

    fill_pref_ui(vbox_opts);
    
    gtk_window_present (GTK_WINDOW(main_window));
}

int main(int argc, char *argv[])
{
    if ( argc != 2 ) {
        fprintf(stderr, "Usage: %s <demo>\n", argv[0]);
        return(1);
    }
    product = argv[1];

    GtkApplication *app;
    int status;

    app = gtk_application_new ("com.lokigames.loki_demos.config", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect (app, "activate", G_CALLBACK (app_activate), NULL);
    status = g_application_run (G_APPLICATION(app), argc-1, argv);
    g_object_unref (app);

    return(status);
}
