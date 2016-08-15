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

#define TI_UNCOPYABLE(name)         \
    private:                        \
        name(const name&) = delete; \
        name& operator=(const name&) = delete

#define TI_MOVABLE(name) \
    public:              \
        name(name&&) = default

#define TI_WRAP(name, ti_name)                             \
    public:                                                \
        using tickit_type = ti_name;                       \
        inline tickit_type* unwrap() {                     \
            assert(m_ ## name); return m_ ## name.get(); } \
        inline const tickit_type* unwrap() const {         \
            assert(m_ ## name); return m_ ## name.get(); } \
    private:                                               \
        using deleter = std::function<void(ti_name*)>;     \
        explicit name(tickit_type* ptr, deleter d):        \
            m_ ## name(ptr, d) { assert(ptr); assert(d);   \
        }                                                  \
        static void no_delete(tickit_type*) { }            \
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


class render_buffer {
    TI_UNCOPYABLE(render_buffer);
    TI_MOVABLE(render_buffer);

public:
    // Generating output.
    render_buffer& write(const std::string&);
    render_buffer& write(long long unsigned);
    render_buffer& write(long long int);

    render_buffer& clear();

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
#undef TI_WRAP

#endif /* !TI_HH */