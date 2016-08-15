/*
 * ti.hh
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef TI_HH
#define TI_HH

#include <string>
#include <memory>
#include <cassert>
#include <experimental/optional>
using std::experimental::optional;

struct TickitRenderBuffer;
struct TickitRectSet;
struct TickitWindow;
struct TickitTerm;
struct TickitPen;


namespace ti {

using uint = unsigned int;

void _track(const char *tname, const char* ti_tname, void* ptr, const char *what);

#define TI_UNCOPYABLE(name)         \
    private:                        \
        name(const name&) = delete; \
        name& operator=(const name&) = delete

#define TI_MOVABLE(name) \
    public:              \
        name(name&&) = default

#define TI_WRAP__DEBUG(name, ti_name) \
        static void track_delete(tickit_type* ptr);        \
        static void no_delete(tickit_type* ptr) {          \
            ti::_track(#name, #ti_name, ptr, "kept"); }    \
        explicit name(tickit_type* ptr, deleter d):        \
            m_ ## name(ptr, d) { assert(ptr); assert(d);   \
            ti::_track(#name, #ti_name, ptr, "wrap"); }

#define TI_WRAP__NODEBUG(name, ti_name)                    \
        static void no_delete(tickit_type*) { }            \
        explicit name(tickit_type* ptr, deleter d):        \
            m_ ## name(ptr, d) { assert(ptr); assert(d); }

#if defined(TI_TRACE_POINTERS) && TI_TRACE_POINTERS
# define TI_WRAP_ TI_WRAP__DEBUG
#else
# define TI_WRAP_ TI_WRAP__NODEBUG
#endif

#define TI_WRAP(name, ti_name)                             \
    public:                                                \
        using tickit_type = ti_name;                       \
        inline tickit_type* unwrap() {                     \
            assert(m_ ## name); return m_ ## name.get(); } \
        inline const tickit_type* unwrap() const {         \
            assert(m_ ## name); return m_ ## name.get(); } \
    private:                                               \
        using deleter = std::function<void(ti_name*)>;     \
        TI_WRAP_(name, ti_name)                            \
        std::unique_ptr<tickit_type, deleter> m_ ## name


// Forward declarations.
template <typename T> struct event_handler;
class render_buffer;


template <typename T>
class event_binding_base {
    TI_MOVABLE(event_binding_base);

public:
    void unbind();

private:
    event_binding_base(T& object, int event_id, int unbind_event_id)
        : m_object(object), m_event_id(event_id), m_unbind_event_id(unbind_event_id) { }

    T&  m_object;
    int m_event_id;
    int m_unbind_event_id;

    template <typename H> friend struct event_handler;
};


template <typename T, typename E>
struct event_base
{
    TI_UNCOPYABLE(event_base);

public:
    using emitter_type = T;
    using event_type   = E;
    using functor_type = std::function<bool(event_type&)>;

private:
    event_base() = default;

    template <typename H> struct event_handler;
    friend emitter_type;
    friend event_type;
};


#define TI_EVENT_BASE(name) \
    template <typename T> \
    struct name : public event_base<T, name<T>>


TI_EVENT_BASE(expose_event_base)
{
    render_buffer& render;
    uint top, left, lines, columns;

    expose_event_base(render_buffer& rb, uint t, uint l, uint ll, uint cc)
        : render(rb), top(t), left(l), lines(ll), columns(cc) { }
};


TI_EVENT_BASE(geometry_change_event_base)
{
    uint old_top, old_left, old_lines, old_columns;
    uint top, left, lines, columns;

    geometry_change_event_base(uint ot, uint ol, uint oll, uint occ,
                               uint nt, uint nl, uint nll, uint ncc)
        : old_top(ot), old_left(ol), old_lines(oll), old_columns(occ)
        , top(nt), left(nl), lines(nll), columns(ncc) { }
};


class terminal {
    TI_UNCOPYABLE(terminal);
    TI_MOVABLE(terminal);

public:
    enum mouse { off, click, drag, move };
    enum screen { normal, alt, altscreen = alt };

    terminal();

    terminal& flush();
    terminal& clear();
    terminal& wait_ready(uint msec = 50);
    terminal& wait_input(int msec = -1);
    terminal& set(enum mouse mode);
    terminal& set(enum screen mode);

    terminal& write(const std::string&);
    terminal& write(long long unsigned);
    terminal& write(long long int);

private:
    TI_WRAP(terminal, TickitTerm);
    friend class window;
};


template <typename T>
static inline terminal& operator<<(terminal& term, const T& v)
{
    return term.write(v);
}


class pen {
public:
    enum attr {
        attr_fg,
        attr_bg,
        attr_bold,
        attr_italic,
        attr_underline,
        attr_reverse,
        attr_strike,
        attr_blink
    };
    using attr_reg = std::pair<enum attr, int>;

    static const attr_reg bold, italic, underline, reverse, strike, blink;

    static inline attr_reg fg(int color = -1) { return { attr_fg, color }; }
    static inline attr_reg bg(int color = -1) { return { attr_bg, color }; }

    pen(std::initializer_list<attr_reg> l);
    pen(const pen& other);

    inline pen(pen&& other): m_pen(nullptr) { *this = std::move(other); }

    ~pen();

    pen& operator=(pen&& other);

    pen& operator=(const pen& other) {
        if (this != &other) {
            pen copy(other);
            std::swap(m_pen, copy.m_pen);
        }
        return *this;
    }

    bool operator==(const pen& other) const;

    pen& set(attr tag, int value = -1);
    inline pen& set(const attr_reg& a) {
        return set(a.first, a.second);
    }

    enum copy_mode { copy_normal, overwrite };
    pen& copy_from(const pen& other, copy_mode mode = copy_mode::copy_normal);

    inline pen copy() const {
        pen newpen {};
        newpen.copy_from(*this);
        return newpen;
    }

    inline TickitPen* unwrap() { assert(m_pen); return m_pen; }
    inline const TickitPen* unwrap() const { assert(m_pen); return m_pen; }

private:
    // XXX: Note that TickitPen keeps its own reference count, so this does
    //      not use std::shared_ptr; instead we define the needed operators
    //      to call tickit_pen_ref() and tickit_pen_unref().
    TickitPen *m_pen;

    pen(TickitPen *p);
};


class render_buffer {
    TI_UNCOPYABLE(render_buffer);
    TI_MOVABLE(render_buffer);

public:
    // Generating output.
    render_buffer& write(const std::string&);
    render_buffer& write(long long unsigned);
    render_buffer& write(long long int);

    render_buffer& clear();
    render_buffer& clear(uint line, uint col, uint columns);

    // Paint state.
    render_buffer& save();
    render_buffer& save_pen();
    render_buffer& restore();
    render_buffer& set_pen(const pen& p);

    // Movement.
    render_buffer& at(uint line, uint col);

private:
    TI_WRAP(render_buffer, TickitRenderBuffer);

    template <typename H> friend struct event_handler;
};


template <typename T>
static inline render_buffer& operator<<(render_buffer& rb, const T& v)
{
    return rb.write(v);
}


class window {
    TI_UNCOPYABLE(window);
    TI_MOVABLE(window);

public:
    using event_binding = event_binding_base<window>;
    using expose_event = expose_event_base<window>;
    using geometry_change_event = geometry_change_event_base<window>;

    enum flags {
        no_flags    = 0,
        hidden      = 1 << 0,
        lowest      = 1 << 1,
        root_parent = 1 << 2,
        steal_input = 1 << 3,
        popup       = 1 << 4,
    };
    window(terminal& term);
    window(window& parent,
           uint top, uint left, uint lines, uint cols,
           enum flags flags = no_flags);

    window root() const;
    optional<window> parent() const;

    window& expose();
    window& flush();

    uint top() const;
    uint left() const;
    uint lines() const;
    uint columns() const;

    window& set_position(uint line, uint col);
    window& set_geometry(uint line, uint col, uint lines, uint columns);

    event_binding on_expose(expose_event::functor_type f);
    event_binding on_geometry_change(geometry_change_event::functor_type f);

private:
    TI_WRAP(window, TickitWindow);
};


} // namespace ti


#undef TI_EVENT_BASE
#undef TI_UNCOPYABLE
#undef TI_MOVABLE
#undef TI_WRAP__NODEBUG
#undef TI_WRAP__DEBUG
#undef TI_WRAP_
#undef TI_WRAP

#endif /* !TI_HH */
