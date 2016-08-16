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

void _track(const char *tname, const char* ti_tname, void* ptr, const char *what)
{
    if (tickit_debug_enabled) {
        tickit_debug_logf("Tp", "%s %p (%s)", what, ptr, ti_tname);
    }
}

#if defined(TI_TRACE_POINTERS) && TI_TRACE_POINTERS
void terminal::track_delete(TickitTerm* ptr)
{
    ti::_track("terminal", "TickitTerm", ptr, "free");
    tickit_term_destroy(ptr);
}
void window::track_delete(TickitWindow* ptr)
{
    ti::_track("window", "TickitWindow", ptr, "free");
    tickit_window_destroy(ptr);
}
void render_buffer::track_delete(TickitRenderBuffer* ptr)
{
    ti::_track("render_buffer", "TickitRenderBuffer", ptr, "free");
    tickit_renderbuffer_destroy(ptr);
}
# define terminal_free       terminal::track_delete
# define window_free         window::track_delete
# define render_buffer_free  render_buffer::track_delete
# define trace_pointer       ti::_track
#else
# define terminal_free       tickit_term_destroy
# define window_free         tickit_window_destroy
# define render_buffer_free  tickit_renderbuffer_destroy
# define trace_pointer(a, b, c, d)  ((void)0)
#endif


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


template <typename TT, typename T> static inline TT to_tickit(T r);

template <>
TickitRect to_tickit(const rect& r)
{
    TickitRect tr = {
        .top = u2i(r.top),
        .left = u2i(r.left),
        .lines = u2i(r.lines),
        .cols = u2i(r.columns),
    };
    return tr;
}


template <typename T, typename TT> static inline T from_tickit(TT r);

template <>
rect& from_tickit(TickitRect* r)
{
    assert(r->top >= 0);
    assert(r->left >= 0);
    assert(r->lines >= 0);
    assert(r->cols >= 0);
    return *reinterpret_cast<rect*>(r);
}

template <>
rect from_tickit(const TickitRect& r)
{
    return { i2u(r.top), i2u(r.left), i2u(r.lines), i2u(r.cols) };
}


terminal::terminal()
    : terminal(tickit_term_open_stdio(), terminal_free)
{
    trace_pointer("terminal", "TickitTerm", unwrap(), "   +");
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


const pen::attr_reg pen::bold      = { pen::attr_bold,      1 };
const pen::attr_reg pen::underline = { pen::attr_underline, 1 };
const pen::attr_reg pen::italic    = { pen::attr_italic,    1 };
const pen::attr_reg pen::blink     = { pen::attr_blink,     1 };
const pen::attr_reg pen::reverse   = { pen::attr_reverse,   1 };
const pen::attr_reg pen::strike    = { pen::attr_strike,    1 };

pen::pen(std::initializer_list<pen::attr_reg> l)
    : m_pen(tickit_pen_new())
{
    for (auto reg: l) set(reg);
}

pen::pen(const pen& other)
    : m_pen(tickit_pen_ref(other.m_pen))
{
}

pen::~pen()
{
    tickit_pen_unref(m_pen);
    m_pen = nullptr;
}

pen& pen::operator=(pen&& other)
{
    if (this != &other) {
        if (m_pen) {
            tickit_pen_unref(m_pen);
            m_pen = nullptr;
        }
        std::swap(m_pen, other.m_pen);
    }
    return *this;
}

bool pen::operator==(const pen& other) const {
    return this == &other
        || m_pen == other.m_pen
        || tickit_pen_equiv(m_pen, other.m_pen);
}

static inline TickitPenAttr to_tickit(pen::attr attr)
{
    switch (attr) {
        case pen::attr_fg:        return TICKIT_PEN_FG;
        case pen::attr_bg:        return TICKIT_PEN_BG;
        case pen::attr_bold:      return TICKIT_PEN_BOLD;
        case pen::attr_underline: return TICKIT_PEN_UNDER;
        case pen::attr_italic:    return TICKIT_PEN_ITALIC;
        case pen::attr_reverse:   return TICKIT_PEN_REVERSE;
        case pen::attr_strike:    return TICKIT_PEN_STRIKE;
        case pen::attr_blink:     return TICKIT_PEN_BLINK;
    }
    assert(false);
    return TICKIT_PEN_FG;
}

pen& pen::set(pen::attr tag, int value)
{
    TickitPenAttr attr_tag = to_tickit(tag);
    switch (tickit_pen_attrtype(attr_tag)) {
        case TICKIT_PENTYPE_BOOL:
            tickit_pen_set_bool_attr(m_pen, attr_tag, value != 0);
            break;
        case TICKIT_PENTYPE_INT:
            tickit_pen_set_int_attr(m_pen, attr_tag, value);
            break;
        case TICKIT_PENTYPE_COLOUR:
            tickit_pen_set_colour_attr(m_pen, attr_tag, value);
            break;
    }
    return *this;
}

pen& pen::copy_from(const pen& other, pen::copy_mode mode)
{
    tickit_pen_copy(m_pen, other.m_pen, mode == pen::copy_mode::overwrite);
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

render_buffer& render_buffer::clear(const rect& r)
{
    auto tr(to_tickit<TickitRect, const rect&>(r));
    tickit_renderbuffer_eraserect(unwrap(), &tr);
    return *this;
}

render_buffer& render_buffer::clear(uint line, uint col, uint cols)
{
    tickit_renderbuffer_erase_at(unwrap(), u2i(line), u2i(col), u2i(cols));
    return *this;
}

render_buffer& render_buffer::save()
{
    tickit_renderbuffer_save(unwrap());
    return *this;
}

render_buffer& render_buffer::save_pen()
{
    tickit_renderbuffer_savepen(unwrap());
    return *this;
}

render_buffer& render_buffer::restore()
{
    tickit_renderbuffer_restore(unwrap());
    return *this;
}

render_buffer& render_buffer::set_pen(const pen& p)
{
    tickit_renderbuffer_setpen(unwrap(), p.unwrap());
    return *this;
}

render_buffer& render_buffer::add_pen(const pen& p)
{
    tickit_renderbuffer_savepen(unwrap());
    tickit_renderbuffer_setpen(unwrap(), p.unwrap());
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

window::window(terminal& term)
    : window(tickit_window_new_root(term.unwrap()), window_free)
{
    trace_pointer("window", "TickitWindow", unwrap(), "   +");
}

window::window(window& parent,
               const rect& r,
               enum window::flags flags)
    : window(tickit_window_new(parent.unwrap(),
                               to_tickit<TickitRect, const rect&>(r),
                               to_tickit(flags)),
             window_free)
{
    trace_pointer("window", "TickitWindow", unwrap(), "   +");
}

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

window& window::set_geometry(const rect& r)
{
    tickit_window_set_geometry(unwrap(), to_tickit<TickitRect, const rect&>(r));
    return *this;
}

rect window::absolute_geometry() const
{
    return from_tickit<rect, const TickitRect&>(tickit_window_get_abs_geometry(unwrap()));
}

rect window::geometry() const
{
    return from_tickit<rect, const TickitRect&>(tickit_window_get_geometry(unwrap()));
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
            rb, from_tickit<rect&>(&info->rect)
        };
        return handle(event);
    }

    inline bool run(TickitWindow*, TickitGeomchangeEventInfo *info) {
        window::geometry_change_event event {
            from_tickit<rect&>(&info->oldrect),
            from_tickit<rect&>(&info->rect)
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
