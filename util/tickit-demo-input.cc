/*
 * tickit-demo-key.cc
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "tickit.hh"
#include <signal.h>

static bool still_running = true;

static void signal_int(int)
{
    still_running = false;
}


int main()
{
    ti::init_debug();

    ti::term tt(ti::term::STDIO);
    tt.wait_ready(50)
        .ctl(ti::term::mouse::DRAG)
        .ctl(ti::term::screen::ALT)
        .clear();

    ti::window root(tt);
    ti::window keywin(root,   { 2, 2, 3, root.cols() - 4 });
    ti::window mousewin(root, { 8, 2, 3, root.cols() - 4 });

    struct {
        std::string                last_key;
        enum ti::key_event::type   last_key_type;
        ti::key_event::mod         last_key_mods;

        int                        last_mouse;
        enum ti::mouse_event::type last_mouse_type;
        ti::mouse_event::mod       last_mouse_mods;
        size_t                     last_mouse_line;
        size_t                     last_mouse_col;
    } info;

    keywin.bind_expose([&info](ti::expose_event& event) {
        static const ti::pen bold { ti::pen::bold };
        event.render_buffer
            .erase(event.rect)
            .goto_position(0, 0)
            .save_pen()
            .set_pen(bold)
            .text("Key:")
            .restore()
            .goto_position(2, 2);
        switch (info.last_key_type) {
            case ti::key_event::KEY:
                event.render_buffer.text("key '");
                break;
            case ti::key_event::TEXT:
                event.render_buffer.text("text '");
                break;
            default: return;
        }

        event.render_buffer.text(info.last_key);

        if (info.last_key_mods) {
            event.render_buffer.text("' modifiers");
            if (info.last_key_mods & ti::key_event::SHIFT)
                event.render_buffer.text(" SHIFT");
            if (info.last_key_mods & ti::key_event::CTRL)
                event.render_buffer.text(" CTRL");
            if (info.last_key_mods & ti::key_event::ALT)
                event.render_buffer.text(" ALT");
        } else {
            event.render_buffer.text("'");
        }
    });

    mousewin.bind_expose([&info](ti::expose_event& event) {
        static const ti::pen bold { ti::pen::bold };
        event.render_buffer
            .erase(event.rect)
            .goto_position(0, 0)
            .save_pen()
            .set_pen(bold)
            .text("Mouse:")
            .restore()
            .goto_position(2, 2);
        switch (info.last_mouse_type) {
            case ti::mouse_event::PRESS:
                event.render_buffer.text("press   ");
                break;
            case ti::mouse_event::DRAG:
                event.render_buffer.text("drag    ");
                break;
            case ti::mouse_event::RELEASE:
                event.render_buffer.text("release ");
                break;
            case ti::mouse_event::WHEEL:
                event.render_buffer.text("wheel   ");
                break;
            default: return;
        }
    });

    root.bind_key([&info, &keywin](ti::key_event& event) {
        if (event.type == ti::key_event::KEY && event.data == "C-c") {
            still_running = false;
            return;
        }
        info.last_key_type = event.type;
        info.last_key_mods = event.modifiers;
        info.last_key = event.data;
        keywin.expose();
    });

    root.bind_mouse([&info, &mousewin](ti::mouse_event& event) {
        info.last_mouse = event.button;
        info.last_mouse_type = event.type;
        info.last_mouse_mods = event.modifiers;
        info.last_mouse_line = event.line;
        info.last_mouse_col  = event.col;
        mousewin.expose();
    });

    root.focus();

    signal(SIGINT, signal_int);
    while (still_running) {
        root.flush();
        tt.wait_input();
    }
}

