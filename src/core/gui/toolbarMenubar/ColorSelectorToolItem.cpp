#include "ColorSelectorToolItem.h"

#include <gtk/gtk.h>

#include "control/actions/ActionDatabase.h"
#include "gui/toolbarMenubar/icon/ColorIcon.h"  // for ColorIcon
#include "util/GtkUtil.h"                       // for fixActionableInitialSensitivity
#include "util/gtk4_helper.h"                   // for gtk_button_set_child
#include "util/i18n.h"                          // for _

constexpr int ICON_SIZE = 22;

ColorSelectorToolItem::ColorSelectorToolItem(ActionDatabase& db):
        AbstractToolItem("COLOR_SELECT", Category::COLORS), gAction(db.getAction(Action::TOOL_COLOR)) {}

auto ColorSelectorToolItem::createItem(bool) -> vn::util::WidgetSPtr {
    GtkWidget* btn = gtk_button_new();
    gtk_widget_set_can_focus(btn, false);  // todo(gtk4) not necessary anymore
    vn::util::GVariantSPtr v(g_action_get_state(G_ACTION(gAction.get())), vn::util::adopt);
    Color initialColor = getGVariantValue<Color>(v.get());
    gtk_button_set_child(GTK_BUTTON(btn), ColorIcon::newGtkImage(initialColor, ICON_SIZE, false));

    gtk_widget_set_tooltip_text(btn, this->getToolDisplayName().c_str());
    gtk_widget_set_sensitive(btn, g_action_get_enabled(G_ACTION(gAction.get())));
    gtk_actionable_set_action_name(GTK_ACTIONABLE(btn),
                                   (std::string("win.") + Action_toString(Action::SELECT_COLOR)).c_str());

    // The color follows the value of the GAction state
    // NB: we use the notify::state signal because "change-state" is not raised by g_simple_action_set_state()
    g_signal_connect_object(gAction.get(), "notify::state", G_CALLBACK(+[](GObject* a, GParamSpec*, gpointer btn) {
                                vn::util::GVariantSPtr state(g_action_get_state(G_ACTION(a)), vn::util::adopt);
                                auto c = getGVariantValue<Color>(state.get());
                                gtk_button_set_child(GTK_BUTTON(btn), ColorIcon::newGtkImage(c, ICON_SIZE, false));
                            }),
                            btn, GConnectFlags(0));

    // Make a proxy for GtkToolbar
    GtkToolItem* it = gtk_tool_item_new();
    gtk_container_add(GTK_CONTAINER(it), btn);
    /// Makes a proxy item for the toolbar's overflow menu
    auto createProxy = [&]() {
        constexpr int PROXY_ICON_SIZE = 16;
        GtkWidget* proxy = gtk_menu_item_new();
        auto* box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_container_add(GTK_CONTAINER(proxy), box);
        gtk_box_append(GTK_BOX(box), ColorIcon::newGtkImage(Colors::gray, PROXY_ICON_SIZE, false));
        gtk_box_append(GTK_BOX(box), gtk_label_new(getToolDisplayName().c_str()));
        gtk_actionable_set_action_name(GTK_ACTIONABLE(proxy),
                                       (std::string("win.") + Action_toString(Action::SELECT_COLOR)).c_str());
        vn::util::gtk::fixActionableInitialSensitivity(GTK_ACTIONABLE(proxy));
        return proxy;
    };
    gtk_tool_item_set_proxy_menu_item(it, "", createProxy());
    return vn::util::WidgetSPtr(GTK_WIDGET(it), vn::util::adopt);
}

auto ColorSelectorToolItem::getToolDisplayName() const -> std::string { return _("Select color"); }

auto ColorSelectorToolItem::getNewToolIcon() const -> GtkWidget* {
    return ColorIcon::newGtkImage(Colors::gray, ICON_SIZE, false);
}
