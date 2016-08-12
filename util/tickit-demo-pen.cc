/*
 * tickit-demo-pen.cc
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

    struct {
        std::string name;
        ti::pen fg, fg_hi, bg, bg_hi;
    } pens[] = {
        { "red",    { ti::pen::fg(1) },
                    { ti::pen::fg(1), ti::pen::bold },
                    { ti::pen::bg(1) },
                    { ti::pen::bg(1), ti::pen::bold } },
        { "blue",   { ti::pen::fg(4) },
                    { ti::pen::fg(4), ti::pen::bold },
                    { ti::pen::bg(4) },
                    { ti::pen::bg(4), ti::pen::bold } },
        { "green",  { ti::pen::fg(2) },
                    { ti::pen::fg(2), ti::pen::bold },
                    { ti::pen::bg(2) },
                    { ti::pen::bg(2), ti::pen::bold } },
        { "yellow", { ti::pen::fg(3) },
                    { ti::pen::fg(3), ti::pen::bold },
                    { ti::pen::bg(3) },
                    { ti::pen::bg(3), ti::pen::bold } },
    };

    ti::term tt(ti::term::STDIO);
    tt.wait_ready(50).clear();

    ti::window root(tt);
    root.bind_expose([&pens](ti::expose_event& event) {
        event.render_buffer.erase(event.rect).goto_position(0, 0);
        for (unsigned i = 0; i < 4; i++) {
            event.render_buffer
                .save_pen()
                .set_pen(pens[i].fg)
                .text("fg ")
                .text(pens[i].name)
                .restore()
                .text("      ");
        }
    });

    signal(SIGINT, signal_int);
    while (still_running) {
        root.flush();
        tt.wait_input();
    }
}
