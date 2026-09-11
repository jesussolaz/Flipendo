# SPDX-FileCopyrightText: 2024 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import Menu


def draw_action_and_slot_selector_for_id(layout, animated_id):
    """
    Draw the action and slot selector for an ID, using the given layout.

    The ID must be an animatable ID.

    Note that the slot selector is only drawn when the ID has an assigned
    Action.
    """

    layout.template_action(animated_id, new="action.new", unlink="action.unlink")

    adt = animated_id.animation_data
    if not adt or not adt.action:
        return

    # Only show the slot selector when a layered Action is assigned.
    if adt.action.is_action_layered:
        layout.context_pointer_set("animated_id", animated_id)
        layout.template_search(
            adt, "action_slot",
            adt, "action_suitable_slots",
            new="anim.slot_new_for_id",
            unlink="anim.slot_unassign_from_id",
        )


classes = (
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
