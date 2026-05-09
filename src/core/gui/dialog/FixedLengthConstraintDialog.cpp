/*
 * VertexNote
 *
 * Simple dialog for editing a fixed-length geometry constraint.
 */

#include "FixedLengthConstraintDialog.h"

#include <algorithm>
#include <functional>

#include "util/gtk4_helper.h"
#include "util/i18n.h"

namespace vn::popup {
namespace {

class FixedLengthConstraintDialogController {
public:
    FixedLengthConstraintDialogController(GtkWindow* parent, double initialValue, std::function<void(double)> callback):
            callback(std::move(callback)),
            dialog(GTK_DIALOG(gtk_dialog_new_with_buttons(_("Edit Fixed Length"), parent,
                                                          static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL |
                                                                                      GTK_DIALOG_DESTROY_WITH_PARENT),
                                                          _("_Cancel"), GTK_RESPONSE_CANCEL, _("_Apply"),
                                                          GTK_RESPONSE_OK, nullptr))) {
        auto* content = gtk_dialog_get_content_area(dialog);
        auto* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
        gtk_container_set_border_width(GTK_CONTAINER(box), 12);
        gtk_box_append(GTK_BOX(content), box);

        auto* label = gtk_label_new(_("Length"));
        gtk_widget_set_halign(label, GTK_ALIGN_START);
        gtk_box_append(GTK_BOX(box), label);

        valueInput = GTK_SPIN_BUTTON(gtk_spin_button_new_with_range(0.01, 1000000.0, 0.1));
        gtk_spin_button_set_digits(valueInput, 2);
        gtk_spin_button_set_value(valueInput, std::max(0.01, initialValue));
        gtk_box_append(GTK_BOX(box), GTK_WIDGET(valueInput));

        auto* applyButton = gtk_dialog_get_widget_for_response(dialog, GTK_RESPONSE_OK);
        gtk_widget_grab_default(applyButton);
        gtk_widget_grab_focus(GTK_WIDGET(valueInput));

        g_signal_connect(dialog, "response", G_CALLBACK(+[](GtkDialog* dialog, int response, gpointer data) {
                             auto* self = static_cast<FixedLengthConstraintDialogController*>(data);
                             if (response == GTK_RESPONSE_OK) {
                                 self->callback(gtk_spin_button_get_value(self->valueInput));
                             }
                             gtk_window_close(GTK_WINDOW(dialog));
                         }),
                         this);
        g_signal_connect(dialog, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
                             delete static_cast<FixedLengthConstraintDialogController*>(data);
                         }),
                         this);

        gtk_widget_show_all(GTK_WIDGET(dialog));
    }

private:
    std::function<void(double)> callback;
    GtkDialog* dialog = nullptr;
    GtkSpinButton* valueInput = nullptr;
};

}  // namespace

void FixedLengthConstraintDialog::show(GtkWindow* parent, double initialValue, std::function<void(double)> callback) {
    new FixedLengthConstraintDialogController(parent, initialValue, std::move(callback));
}

}  // namespace vn::popup
