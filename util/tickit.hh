/*
 * tickit.hh
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

#ifndef TICKIT_HH
#define TICKIT_HH

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <initializer_list>
#include <experimental/optional>
using std::experimental::optional;

struct TickitRenderBuffer;
struct TickitRectSet;
struct TickitWindow;
struct TickitTerm;
struct TickitPen;

namespace ti {

void init_debug();


template <typename T> struct event_handler_info;
struct render_buffer;
struct window;
struct rect;


struct expose_event {
    using handler = std::function<void(struct expose_event&)>;

    struct window& window;
    struct render_buffer& render_buffer;
    const struct rect& rect;

    expose_event() = delete;
};

struct focus_event {
    using handler = std::function<void(struct focus_event&)>;

    enum direction { IN, OUT };

    struct window& window;
    struct window& target;
    enum direction direction;

    focus_event() = delete;
};

struct geometry_change_event {
    using handler = std::function<void(struct geometry_change_event&)>;

    struct window& window;
    const struct rect& old_geometry;
    const struct rect& new_geometry;

    geometry_change_event() = delete;
};

struct key_event {
    using handler = std::function<void(struct key_event&)>;

    enum type {
        KEY,
        TEXT,
    };
    enum mod {
        // Same values as in tickit.h
        SHIFT = 1 << 0,
        ALT   = 1 << 1,
        CTRL  = 1 << 2,
    };

    struct window& window;
    enum type type;
    enum mod modifiers;
    std::string data;

    key_event() = delete;
};

struct mouse_event {
    using handler = std::function<void(struct mouse_event&)>;
    using mod = key_event::mod;

    enum type {
        PRESS = 1,
        DRAG,
        RELEASE,
        WHEEL,
        DRAG_START = 0x101,
        DRAG_OUTSIDE,
        DRAG_DROP,
        DRAG_STOP
    };
    enum wheel {
        UP = 1,
        DOWN,
    };

    struct window& window;
    int button;
    enum type type;
    mod modifiers;
    size_t line, col;

    mouse_event() = delete;
};


template <typename T>
struct event_binding {
private:
    event_binding() = default;
    event_binding(int id, int unbind_id)
        : m_id(id), m_unbind_id(unbind_id) { }

    int m_id, m_unbind_id;

    template <typename T2> friend struct event_handler_info;
    friend T;
};


struct rect {
    // Copy constructor.
    explicit inline rect(const rect& r) = default;

    // Move constructor.
    inline rect(rect&& r) = default;

    inline rect(int top, int left, int lines, int cols)
        : m_top(top), m_left(left), m_lines(lines), m_cols(cols) { }

    enum bounded_guard { BOUNDED };
    explicit rect(bounded_guard, int top, int left, int bottom, int right);

    inline int top()   const { return m_top;   }
    inline int left()  const { return m_left;  }
    inline int lines() const { return m_lines; }
    inline int cols()  const { return m_cols;  }

    int bottom() const;
    int right() const;

    bool contains(const rect& r) const;
    bool intersects(const rect& r) const;

    optional<const rect> intersect(const rect& r) const;

    // TODO: tickit_rect_add(), may return one-to-three rectangles. Probably
    //       the thing to do is to return a stack-allocated std::vector().
    //       Same for tickit_rect_subtract()

    const rect translate(int downward, int rightward) const;

private:
    // The default constructor is used internally only.
    explicit inline rect() = default;

    int m_top;
    int m_left;
    int m_lines;
    int m_cols;
};


struct rect_set {
    explicit rect_set();

    rect_set& add(const rect& r);
    rect_set& subtract(const rect& r);
    rect_set& clear();
    rect_set& translate(int downward, int rightward);

    size_t size() const;
    std::vector<rect> rects() const;

    bool contains(const rect& r) const;
    bool intersects(const rect& r) const;

private:
    std::shared_ptr<TickitRectSet> m_rectset;
};


struct pen {
    enum attr { FG, BG, BOLD, UNDERLINE, ITALIC, REVERSE, STRIKE, BLINK };
    using attr_reg = std::pair<enum attr, int>;

    static const attr_reg bold, underline, italic, reverse, strike, blink;

    static attr_reg fg(int color = -1) { return { attr::FG, color }; }
    static attr_reg bg(int color = -1) { return { attr::BG, color }; }

    pen(std::initializer_list<attr_reg> l);

    pen(const pen& other);  // Copy constructor.

    // Move constructor. Reuses the move assignment operator.
    inline pen(pen&& other): m_pen(nullptr) { *this = std::move(other); }

    ~pen();

    pen& operator=(pen&& other);  // Move assignment.

    // Copy assignment. Uses the copy-and-swap pattern.
    inline pen& operator=(const pen& other) {
        if (this != &other) {
            pen other_copy(other);
            std::swap(m_pen, other_copy.m_pen);
        }
        return *this;
    }

    // Comparison.
    bool operator==(const pen& other) const;

    bool is_empty() const;
    bool is_non_default() const;

    enum has_mode { HAS_NORMAL, NON_DEFAULT };
    bool has(attr tag, has_mode mode = HAS_NORMAL) const;

    pen& clear(attr tag);
    pen& clear();

    pen& set(attr tag, int value = -1);
    inline pen& set(const attr_reg& attr) {
        return set(attr.first, attr.second);
    }

    template <typename T> T get(attr a) const;

    enum copy_mode { COPY_NORMAL, OVERWRITE };
    pen& copy_from(const pen& other, copy_mode mode = COPY_NORMAL);

    inline pen copy() const {
        pen new_pen {};
        new_pen.copy_from(*this);
        return new_pen;
    }

private:
    // XXX: Note that TickitPen keeps its own reference count, so this does
    //      not use std::shared_ptr; instead we define the needed operators
    //      to call tickit_pen_ref() and tickit_pen_unref().
    TickitPen *m_pen;

    pen(TickitPen *p);

    friend struct render_buffer;
    friend struct window;
    friend struct term;
};

template <> bool pen::get<bool>(pen::attr a) const;
template <> int  pen::get<int>(pen::attr a) const;

struct render_buffer {
    explicit render_buffer(size_t lines, size_t cols);

    render_buffer& reset();

    std::pair<size_t, size_t> size() const;
    inline size_t lines() const { return size().first; }
    inline size_t cols() const { return size().second; }

    render_buffer& save();
    render_buffer& save_pen();
    render_buffer& restore();

    render_buffer& translate(int downward, int rightward);
    render_buffer& clip(const rect& r);
    render_buffer& mask(const rect& r);
    render_buffer& set_pen(const pen& p);

    render_buffer& goto_position(int line, int col);
    render_buffer& clear_position();
    optional<std::pair<size_t, size_t>> get_position() const;

    render_buffer& skip(size_t cols);
    render_buffer& skip_to(size_t col);
    render_buffer& skip_at(size_t line, size_t col, size_t cols);

    // TODO: The formatted text output functions are missing, as they use
    //       varargs. One option might be to allow using operator<< as in
    //       the output streams from the standard library.
    render_buffer& text(const std::string& s);
    render_buffer& text(const std::string& s, size_t len);
    render_buffer& text_at(size_t line, size_t col, const std::string& s);
    render_buffer& text_at(size_t line, size_t col, const std::string& s, size_t len);

    render_buffer& unichar(unsigned long codepoint);
    render_buffer& unichar_at(size_t line, size_t col, unsigned long codepoint);

    enum line_style { SINGLE, DOUBLE, THICK };
    enum line_caps { START, END, BOTH };
    render_buffer& hline_at(size_t line, size_t startcol, size_t endcol,
                            line_style style = line_style::SINGLE,
                            line_caps caps = line_caps::BOTH);
    render_buffer& vline_at(size_t startline, size_t endline, size_t col,
                            line_style style = line_style::SINGLE,
                            line_caps caps = line_caps::BOTH);

    render_buffer& erase(size_t cols);
    render_buffer& erase_to(size_t col);
    render_buffer& erase_at(size_t line, size_t col, size_t cols);
    render_buffer& erase(const rect& r);
    render_buffer& erase();

    // Alias which follows tickit's naming
    inline render_buffer& clear() { return erase(); }

    render_buffer& blit(const render_buffer& source);

private:
    static void noop_deleter(TickitRenderBuffer*) { }

    enum nodelete_guard { NO_DELETE };
    explicit render_buffer(nodelete_guard, TickitRenderBuffer* rb)
        : m_renderbuffer({ rb, noop_deleter }) { }

    inline TickitRenderBuffer* unwrap() { return m_renderbuffer.get(); }

    std::shared_ptr<TickitRenderBuffer> m_renderbuffer;

    template <typename T> friend struct event_handler_info;
    friend struct term;
};


struct term {
    enum stdio_guard { STDIO };

    explicit term(const std::string& term_type);
    explicit term(stdio_guard);
    explicit term();

    std::string term_type() const;

    term& set_input_fd(int fd);
    term& set_output_fd(int fd);

    int input_fd() const;
    int output_fd() const;

    enum set_pen_mode { SET_PEN_NORMAL, PARTIAL };
    term& set_pen(const pen& p, set_pen_mode mode = SET_PEN_NORMAL);

    term& text(const std::string& s, size_t len);
    term& text(const std::string& s);

    term& clear();
    term& flush();
    term& blit(const render_buffer& rb);

    term& wait_ready(unsigned long msec = 50);  // XXX: Does not accept -1!
    term& wait_input(long msec = -1);

    enum mouse { OFF, CLICK, DRAG, MOVE };
    term& ctl(enum mouse mode);

    enum screen { NORMAL, ALT };
    term& ctl(enum screen mode);

private:
    std::shared_ptr<TickitTerm> m_term;

    friend struct window;
};


struct window {
    enum flags {
        NO_FLAGS    = 0,
        HIDDEN      = 1 << 0,
        LOWEST      = 1 << 1,
        ROOT_PARENT = 1 << 2,
        STEAL_INPUT = 1 << 3,
        POPUP       = 1 << 4,
    };

    explicit window(window& parent, const rect& r, flags f = NO_FLAGS);
    explicit window(term& t);

    window(const window& other);

    // Move constructor. Reuses the move assignment operator.
    inline window(window&& other) : m_window() {
        *this = std::move(other);
    }

    window& operator=(window&& other) {
        if (this != &other) {
            std::swap(m_window, other.m_window);
        }
        return *this;
    }

    bool operator==(const window& other) const {
        return m_window.get() == other.m_window.get();
    }

    optional<window> parent() const;
    window root() const;

    size_t top() const;
    size_t left() const;
    size_t lines() const;
    size_t cols() const;
    size_t bottom() const;
    size_t right() const;

    const rect abs_geometry() const;
    const rect geometry() const;
    window& set_geometry(const rect& r);
    window& resize(size_t lines, size_t cols);
    window& reposition(size_t top, size_t left);

    window& raise();
    window& raise_to_front();
    window& lower();
    window& lower_to_back();

    bool is_focused() const;
    window& focus();

    bool is_visible() const;
    window& show();
    window& hide();

    window& expose(const rect& r);
    window& expose();
    window& flush();

    struct pen pen() const;
    window& set_pen(struct pen& p);

    enum scroll_with_children_guard { WITH_CHILDREN };
    window& scroll(int downward, int rightward, scroll_with_children_guard);
    window& scroll(int downward, int rightward);
    window& scroll(const rect& r, int downward, int rightward);
    window& scroll(const rect& r, int downward, int rightward, struct pen& p);

    window& goto_position(size_t line, size_t col);

    enum cursor_shape { BLOCK, IBEAM, UNDERLINE };
    window& set_cursor_shape(cursor_shape shape);
    window& set_cursor_visible(bool visible = true);

    using binding = event_binding<window>;
    binding bind_expose(expose_event::handler f);
    binding bind_focus(focus_event::handler f);
    binding bind_geometry_change(geometry_change_event::handler f);
    binding bind_key(key_event::handler f);
    binding bind_mouse(mouse_event::handler f);
    window& unbind(const binding& b);

private:
    static void noop_deleter(TickitWindow*) { }

    enum nodelete_guard { NO_DELETE };
    explicit window(nodelete_guard, TickitWindow *w)
        : m_window({ w, noop_deleter }) { }

    inline TickitWindow* unwrap() { return m_window.get(); }

    std::shared_ptr<TickitWindow> m_window;

    template <typename T> friend struct event_handler_info;
};

} // namespace ti

#endif /* !TICKIT_HH */
