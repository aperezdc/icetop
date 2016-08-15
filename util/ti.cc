/*
 * ti.cc
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#include "ti.hh"

extern "C" {
#include <tickit.h>
}

#include <limits>

namespace ti {

/*
 * Map object types to their Tickit event callback types.
 */
template <typename T> struct emitter_callback_map { };
#define TI_EMITTER_CALLBACK(emitter_name, callback_name) \
    template <> struct emitter_callback_map<emitter_name> { using type = callback_name; }

TI_EMITTER_CALLBACK(window, TickitWindowEventFn);
TI_EMITTER_CALLBACK(terminal, TickitTermEventFn);


/*
 * Map event types to their Tickit "event info" struct types.
 */
template <typename T> struct event_info_map {
    static const TickitEventType code;
};

#define TI_EVENT_INFO(event_name, event_code, info_name) \
    template <> struct event_info_map<event_name> {      \
        static const TickitEventType code = event_code;  \
        using type = info_name; \
    }

TI_EVENT_INFO(window::expose_event,          TICKIT_EV_EXPOSE,     TickitExposeEventInfo);
TI_EVENT_INFO(window::geometry_change_event, TICKIT_EV_GEOMCHANGE, TickitGeomchangeEventInfo);



struct debug_init {
    debug_init() {
        tickit_debug_init();
    }
};

static debug_init s_debug_init = {};


static inline uint i2u(int v) {
    assert(v >= 0);
    return static_cast<uint>(v);
}

static inline int u2i(uint v) {
    assert(v <= std::numeric_limits<int>::max());
    return static_cast<int>(v);
}


terminal::terminal()
    : terminal(tickit_term_open_stdio(), tickit_term_destroy)
{
}

terminal& terminal::flush() { tickit_term_flush(unwrap()); return *this; }
terminal& terminal::clear() { tickit_term_clear(unwrap()); return *this; }

terminal& terminal::wait_ready(uint msec)
{
    tickit_term_await_started_msec(unwrap(), u2i(msec));
    tickit_term_refresh_size(unwrap());
    tickit_term_observe_sigwinch(unwrap(), true);
    return *this;
}

terminal& terminal::wait_input(int msec)
{
    tickit_term_input_wait_msec(unwrap(), msec);
    return *this;
}

static inline TickitTermMouseMode to_tickit(enum terminal::mouse mode)
{
    switch (mode) {
        case terminal::mouse::off:   return TICKIT_TERM_MOUSEMODE_OFF;
        case terminal::mouse::click: return TICKIT_TERM_MOUSEMODE_CLICK;
        case terminal::mouse::drag:  return TICKIT_TERM_MOUSEMODE_DRAG;
        case terminal::mouse::move:  return TICKIT_TERM_MOUSEMODE_MOVE;
    }
    assert(false);
    return TICKIT_TERM_MOUSEMODE_OFF;
}

terminal& terminal::set(enum terminal::mouse mode)
{
    tickit_term_setctl_int(unwrap(), TICKIT_TERMCTL_MOUSE, to_tickit(mode));
    return *this;
}

terminal& terminal::set(enum terminal::screen mode)
{
    tickit_term_setctl_int(unwrap(), TICKIT_TERMCTL_ALTSCREEN,
                           (mode == screen::alt) ? 1 : 0);
    return *this;
}

terminal& terminal::write(const std::string& s)
{
    tickit_term_printn(unwrap(), s.data(), s.size());
    return *this;
}

terminal& terminal::write(long long unsigned v)
{
    tickit_term_printf(unwrap(), "%llu", v);
    return *this;
}

terminal& terminal::write(long long int v)
{
    tickit_term_printf(unwrap(), "%lli", v);
    return *this;
}

render_buffer& render_buffer::write(const std::string& s)
{
    tickit_renderbuffer_textn(unwrap(), s.data(), s.size());
    return *this;
}

render_buffer& render_buffer::write(long long unsigned v)
{
    tickit_renderbuffer_textf(unwrap(), "%llu", v);
    return *this;
}

render_buffer& render_buffer::write(long long int v)
{
    tickit_renderbuffer_textf(unwrap(), "%lli", v);
    return *this;
}

render_buffer& render_buffer::clear()
{
    tickit_renderbuffer_clear(unwrap());
    return *this;
}

render_buffer& render_buffer::at(uint line, uint col)
{
    tickit_renderbuffer_goto(unwrap(), u2i(line), u2i(col));
    return *this;
}


static inline TickitWindowFlags to_tickit(enum window::flags flags)
{
    uint r = 0;
    if (flags & window::flags::popup) r |= TICKIT_WINDOW_POPUP;
    if (flags & window::flags::hidden) r |= TICKIT_WINDOW_HIDDEN;
    if (flags & window::flags::lowest) r |= TICKIT_WINDOW_LOWEST;
    if (flags & window::flags::root_parent) r |= TICKIT_WINDOW_ROOT_PARENT;
    if (flags & window::flags::steal_input) r |= TICKIT_WINDOW_STEAL_INPUT;
    return static_cast<TickitWindowFlags>(r);
}

static inline TickitRect to_rect(uint top, uint left, uint lines, uint cols)
{
    TickitRect r;
    tickit_rect_init_sized(&r, u2i(top), u2i(left), u2i(lines), u2i(cols));
    return r;
}

window::window(terminal& term)
    : window(tickit_window_new_root(term.unwrap()), tickit_window_destroy) { }

window::window(window& parent,
               uint top, uint left, uint lines, uint cols,
               enum window::flags flags)
    : window(tickit_window_new(parent.unwrap(),
                               to_rect(top, left, lines, cols),
                               to_tickit(flags)),
             tickit_window_destroy) { }

window window::root() const
{
    return window { tickit_window_root(unwrap()), no_delete };
}

optional<window> window::parent() const
{
    if (TickitWindow* w = tickit_window_parent(unwrap()))
        return { window { w, no_delete } };
    return { };
}

window& window::expose()
{
    tickit_window_expose(unwrap(), nullptr);
    return *this;
}

window& window::flush()
{
    tickit_window_flush(unwrap());
    return *this;
}

window& window::set_position(uint line, uint col)
{
    tickit_window_reposition(unwrap(), u2i(line), u2i(col));
    return *this;
}

window& window::set_geometry(uint line, uint col, uint lines, uint columns)
{
    TickitRect r;
    tickit_rect_init_sized(&r, u2i(line), u2i(col), u2i(lines), u2i(columns));
    tickit_window_set_geometry(unwrap(), r);
    return *this;
}

uint window::top() const { return i2u(tickit_window_top(unwrap())); }
uint window::left() const { return i2u(tickit_window_left(unwrap())); }
uint window::lines() const { return i2u(tickit_window_lines(unwrap())); }
uint window::columns() const { return i2u(tickit_window_cols(unwrap())); }



template <typename T>
struct emitter_traits {
    using tickit_emitter_type = typename T::tickit_type;
    using tickit_callback_type = typename emitter_callback_map<T>::type;

    using tickit_bind_function_type = int (* const)(tickit_emitter_type*,
                                                    TickitEventType,
                                                    TickitBindFlags,
                                                    tickit_callback_type,
                                                    void*);

    using tickit_unbind_function_type = void (* const)(tickit_emitter_type*, int);

    static const tickit_bind_function_type   tickit_bind;
    static const tickit_unbind_function_type tickit_unbind;

    static inline int
    bind(T& emitter, TickitEventType ev, tickit_callback_type callback, void* handler_info) {
        return tickit_bind(emitter.unwrap(), ev, static_cast<TickitBindFlags>(0), callback, handler_info);
    }
};

#define EMITTER_BIND_FUNCS(emitter_name, bind_func, unbind_func) \
    template <> emitter_traits<emitter_name>::tickit_bind_function_type \
        emitter_traits<emitter_name>::tickit_bind = bind_func; \
    template <> emitter_traits<emitter_name>::tickit_unbind_function_type \
        emitter_traits<emitter_name>::tickit_unbind = unbind_func

EMITTER_BIND_FUNCS(window, tickit_window_bind_event, tickit_window_unbind_event_id);


template <typename E>
struct event_handler {
    using event_type             = E;
    using emitter_type           = typename event_type::emitter_type;
    using tickit_emitter_type    = typename emitter_type::tickit_type;
    using tickit_callback_type   = typename emitter_callback_map<emitter_type>::type;
    using tickit_event_info_type = typename event_info_map<event_type>::type;

    event_binding_base<emitter_type> bind(emitter_type& emitter) {
        return {
            emitter,
            emitter_traits<emitter_type>::bind(emitter, TICKIT_EV_UNBIND, unbind_callback, this),
            emitter_traits<emitter_type>::bind(emitter, event_info_map<event_type>::code, callback, this)
        };
    }

    inline bool run(TickitWindow*, TickitExposeEventInfo* info) {
        render_buffer rb { info->rb, render_buffer::no_delete };
        window::expose_event event {
            rb, i2u(info->rect.top), i2u(info->rect.left), i2u(info->rect.lines), i2u(info->rect.cols)
        };
        return handle(event);
    }

    inline bool run(TickitWindow*, TickitGeomchangeEventInfo *info) {
        window::geometry_change_event event {
            i2u(info->oldrect.top), i2u(info->oldrect.left), i2u(info->oldrect.lines), i2u(info->oldrect.cols),
            i2u(info->rect.top), i2u(info->rect.left), i2u(info->rect.lines), i2u(info->rect.cols)
        };
        return handle(event);
    }

    static int callback(tickit_emitter_type* e, TickitEventType ev, void* info, void* user) {
        auto handler = reinterpret_cast<event_handler<event_type>*>(user);
        return handler->run(e, reinterpret_cast<tickit_event_info_type*>(info)) ? 1 : 0;
    }

    static int unbind_callback(tickit_emitter_type* e, TickitEventType ev, void* info, void* user) {
        assert(ev & TICKIT_EV_UNBIND);
        delete reinterpret_cast<event_handler<event_type>*>(user);
        return 1;
    }

    typename event_type::functor_type handle;
};


template <typename T>
static inline event_binding_base<typename T::emitter_type>
bind_event(typename T::emitter_type& emitter, typename T::functor_type f)
{
    auto handler = new event_handler<T>{ f };
    return handler->bind(emitter);
}

window::event_binding window::on_expose(window::expose_event::functor_type f)
{
    return bind_event<window::expose_event>(*this, f);
}

window::event_binding window::on_geometry_change(window::geometry_change_event::functor_type f)
{
    return bind_event<window::geometry_change_event>(*this, f);
}

} // namespace ti