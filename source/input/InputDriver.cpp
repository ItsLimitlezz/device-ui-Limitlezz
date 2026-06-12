#include "input/InputDriver.h"
#include "lvgl_private.h"

InputDriver *InputDriver::driver = nullptr;
lv_indev_t *InputDriver::keyboard = nullptr;
lv_indev_t *InputDriver::pointer = nullptr;
lv_indev_t *InputDriver::encoder = nullptr;
lv_indev_t *InputDriver::button = nullptr;
lv_group_t *InputDriver::inputGroup = nullptr;
lv_obj_t *InputDriver::navMenuContainer = nullptr;
lv_obj_t *InputDriver::navMenuButton = nullptr;
void (*InputDriver::navFocusContentCb)(void) = nullptr;
bool (*InputDriver::navScrollContentCb)(int8_t) = nullptr;

InputDriver *InputDriver::instance(void)
{
    if (!driver)
        driver = new InputDriver;
    return driver;
}

InputDriver::~InputDriver(void)
{
    if (keyboard)
        releaseKeyboardDevice();
    if (pointer)
        releasePointerDevice();
}

void InputDriver::setNavRegions(lv_obj_t *menuContainer, lv_obj_t *activeMenuButton, void (*focusContentCb)(void),
                                bool (*scrollContentCb)(int8_t))
{
    navMenuContainer = menuContainer;
    navMenuButton = activeMenuButton;
    navFocusContentCb = focusContentCb;
    navScrollContentCb = scrollContentCb;
}

// is obj (or one of its ancestors) the given container?
static bool objInRegion(lv_obj_t *obj, lv_obj_t *container)
{
    while (obj) {
        if (obj == container)
            return true;
        obj = lv_obj_get_parent(obj);
    }
    return false;
}

// can the group give focus to this object? (mirrors lv_group's focus skip rules)
static bool objFocusable(lv_obj_t *obj)
{
    if (!obj || !lv_obj_is_valid(obj))
        return false;
    if (lv_obj_has_state(obj, LV_STATE_DISABLED))
        return false;
    // skip when the object or any ancestor is hidden
    for (lv_obj_t *p = obj; p; p = lv_obj_get_parent(p)) {
        if (lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN))
            return false;
    }
    return true;
}

// If the focused object is a tabview tab button, switch tabs in the given direction.
// Returns true when it consumed the gesture (a valid neighbouring tab existed).
static bool navTabSwitch(lv_group_t *group, int8_t dir)
{
    lv_obj_t *focused = lv_group_get_focused(group);
    if (!focused)
        return false;
    lv_obj_t *bar = lv_obj_get_parent(focused);
    if (!bar)
        return false;
    lv_obj_t *tv = lv_obj_get_parent(bar);
    if (!tv || !lv_obj_check_type(tv, &lv_tabview_class) || bar != lv_tabview_get_tab_bar(tv))
        return false;
    // index/count in the same by-type space lv_tabview_set_active uses (tab buttons only)
    int32_t cnt = (int32_t)lv_tabview_get_tab_count(tv);
    int32_t next = (int32_t)lv_tabview_get_tab_active(tv) + (dir > 0 ? 1 : -1);
    if (next < 0 || next >= cnt)
        return false; // at an edge: let the caller fall back (menu / nothing)
    lv_tabview_set_active(tv, next, LV_ANIM_ON);
    lv_obj_t *btn = lv_obj_get_child_by_type(bar, next, &lv_button_class);
    if (btn)
        lv_group_focus_obj(btn);
    return true;
}

// Find the tabview whose CONTENT area (an inactive or active page) contains obj.
static lv_obj_t *navTabviewOfContent(lv_obj_t *obj)
{
    for (lv_obj_t *node = obj, *par = lv_obj_get_parent(node); par; node = par, par = lv_obj_get_parent(par)) {
        lv_obj_t *grand = lv_obj_get_parent(par);
        if (grand && lv_obj_check_type(grand, &lv_tabview_class) && par == lv_tabview_get_content(grand))
            return grand;
    }
    return nullptr;
}

bool InputDriver::navFocusContent(void)
{
    if (!inputGroup || !navMenuContainer)
        return false;
    if (lv_group_get_editing(inputGroup))
        return false; // editing a widget: keep legacy key behavior
    // swipe right on a tab bar advances to the next tab (e.g. Settings -> Tools)
    if (navTabSwitch(inputGroup, 1))
        return true;
    if (!navFocusContentCb)
        return false;
    lv_obj_t *focused = lv_group_get_focused(inputGroup);
    if (!focused || !objInRegion(focused, navMenuContainer))
        return false; // not in the menu: nothing to do
    navFocusContentCb();
    return true;
}

bool InputDriver::navFocusMenu(void)
{
    if (!inputGroup || !navMenuContainer)
        return false;
    if (lv_group_get_editing(inputGroup))
        return false; // editing a widget: keep legacy key behavior
    // swipe left on a tab bar goes to the previous tab (Tools -> Settings); only when
    // there is no previous tab does a left swipe fall through to the menu
    if (navTabSwitch(inputGroup, -1))
        return true;
    lv_obj_t *focused = lv_group_get_focused(inputGroup);
    if (!focused || objInRegion(focused, navMenuContainer))
        return false; // already in the menu
    lv_obj_t *target = (navMenuButton && lv_obj_is_valid(navMenuButton)) ? navMenuButton : navMenuContainer;
    lv_group_focus_obj(target);
    return true;
}

bool InputDriver::navBlockBoundary(int8_t dir)
{
    if (!inputGroup || !navMenuContainer)
        return false;
    lv_obj_t *focused = lv_group_get_focused(inputGroup);
    if (!focused)
        return false;
    bool inMenu = objInRegion(focused, navMenuContainer);

    // Scroll-only pages (e.g. the chat history): up/down scroll the page content and
    // never move focus, so the cursor can't land on the text box or escape to the menu.
    // Checked BEFORE the editing guard so it still scrolls even if the text box was
    // toggled into edit mode by a center-press.
    if (!inMenu && navScrollContentCb && navScrollContentCb(dir))
        return true;

    if (lv_group_get_editing(inputGroup))
        return false; // editing another widget (slider/dropdown): enc_diff adjusts its value

    // find the entry of the focused object in the group list
    lv_ll_t *ll = &inputGroup->obj_ll;
    lv_obj_t **i = (lv_obj_t **)_lv_ll_get_head(ll);
    while (i && *i != focused)
        i = (lv_obj_t **)_lv_ll_get_next(ll, i);
    if (!i)
        return false;

    // walk to the object that would receive focus next (with wrap-around)
    lv_obj_t **start = i;
    do {
        i = (dir > 0) ? (lv_obj_t **)_lv_ll_get_next(ll, i) : (lv_obj_t **)_lv_ll_get_prev(ll, i);
        if (!i)
            i = (dir > 0) ? (lv_obj_t **)_lv_ll_get_head(ll) : (lv_obj_t **)_lv_ll_get_tail(ll);
        if (!i || i == start)
            return false;
    } while (!objFocusable(*i));

    // The move would leave the current tab page — either onto a tab button, or onto a
    // widget of an inactive tab page. Redirect to the ACTIVE tab button so the tabs sit
    // at the page edge and the cursor never lands on the wrong tab's content/button.
    lv_obj_t *redirectTv = nullptr;
    for (lv_obj_t *node = *i, *par = lv_obj_get_parent(node); par; node = par, par = lv_obj_get_parent(par)) {
        lv_obj_t *grand = lv_obj_get_parent(par);
        if (!grand || !lv_obj_check_type(grand, &lv_tabview_class))
            continue;
        if (par == lv_tabview_get_content(grand)) {
            // next is a page; redirect only if it's an INACTIVE page
            if ((uint32_t)lv_obj_get_index(node) != lv_tabview_get_tab_active(grand))
                redirectTv = grand;
            break;
        }
        if (par == lv_tabview_get_tab_bar(grand)) {
            // next is a tab button; redirect only if we're leaving this tabview's own content
            if (navTabviewOfContent(focused) == grand)
                redirectTv = grand;
            break;
        }
    }
    if (redirectTv) {
        lv_obj_t *bar = lv_tabview_get_tab_bar(redirectTv);
        lv_obj_t *actBtn = lv_obj_get_child_by_type(bar, lv_tabview_get_tab_active(redirectTv), &lv_button_class);
        if (actBtn && actBtn != focused)
            lv_group_focus_obj(actBtn);
        return true; // consumed
    }

    // block the move when it would cross between menu and page content;
    // switching regions is done with an explicit left/right gesture instead
    if (objInRegion(*i, navMenuContainer) != inMenu)
        return true;
    return false;
}
