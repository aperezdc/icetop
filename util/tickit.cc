/*
 * tickit.cc
 * Copyright (C) 2016 Adrian Perez <aperez@igalia.com>
 *
 * Distributed under terms of the MIT license.
 */

extern "C" {
#include <tickit.h>
}

#include "tickit.hh"
#include <algorithm>
#include <limits>
#include <cassert>


namespace ti {

void init_debug()
{
    tickit_debug_init();
}


static inline size_t size_of_int(int value)
{
    assert(value >= 0);
    return static_cast<size_t>(std::max(0, value));
}

static inline int int_of_size(size_t value)
{
    assert(value <= std::numeric_limits<int>::max());
    return static_cast<int>(std::min(value, static_cast<size_t>(std::numeric_limits<int>::max())));
}

static inline TickitLineStyle to_tickit(render_buffer::line_style style)
{
    switch (style) {
        case render_buffer::line_style::SINGLE: return TICKIT_LINE_SINGLE;
        case render_buffer::line_style::DOUBLE: return TICKIT_LINE_DOUBLE;
        case render_buffer::line_style::THICK:  return TICKIT_LINE_THICK;
    }
    assert(false);
    return TICKIT_LINE_SINGLE;
}

static inline TickitLineCaps to_tickit(render_buffer::line_caps caps)
{
    switch (caps) {
        case render_buffer::line_caps::START: return TICKIT_LINECAP_START;
        case render_buffer::line_caps::END:   return TICKIT_LINECAP_END;
        case render_buffer::line_caps::BOTH:  return TICKIT_LINECAP_BOTH;
    }
    assert(false);
    return TICKIT_LINECAP_BOTH;
}

static inline TickitPenAttr to_tickit(pen::attr attr)
{
    switch (attr) {
        case pen::attr::FG:        return TICKIT_PEN_FG;
        case pen::attr::BG:        return TICKIT_PEN_BG;
        case pen::attr::BOLD:      return TICKIT_PEN_BOLD;
        case pen::attr::UNDERLINE: return TICKIT_PEN_UNDER;
        case pen::attr::ITALIC:    return TICKIT_PEN_ITALIC;
        case pen::attr::REVERSE:   return TICKIT_PEN_REVERSE;
        case pen::attr::STRIKE:    return TICKIT_PEN_STRIKE;
        case pen::attr::BLINK:     return TICKIT_PEN_BLINK;
    }
    assert(false);
    return TICKIT_PEN_FG;
}

#define WINDOW_FLAGS(F)    \
    F (POPUP)       \
    F (HIDDEN)      \
    F (LOWEST)      \
    F (ROOT_PARENT) \
    F (STEAL_INPUT)

static inline TickitWindowFlags to_tickit(window::flags f)
{
    int result = 0;
#define WINDOW_FLAG_TO_TICKIT(flag) \
    if ((f & window::flags:: flag) == window::flags:: flag) \
        result |= TICKIT_WINDOW_ ## flag;

    WINDOW_FLAGS (WINDOW_FLAG_TO_TICKIT)

#undef WINDOW_FLAG_TO_TICKIT
    return static_cast<TickitWindowFlags>(result);
}

static inline TickitCursorShape to_tickit(window::cursor_shape shape)
{
    switch (shape) {
        case window::cursor_shape::BLOCK:     return TICKIT_CURSORSHAPE_BLOCK;
        case window::cursor_shape::IBEAM:     return TICKIT_CURSORSHAPE_LEFT_BAR;
        case window::cursor_shape::UNDERLINE: return TICKIT_CURSORSHAPE_UNDER;
    }
    assert(false);
    return TICKIT_CURSORSHAPE_BLOCK;
}

static inline TickitTermMouseMode to_tickit(term::mouse mode)
{
    switch (mode) {
        case term::mouse::OFF:   return TICKIT_TERM_MOUSEMODE_OFF;
        case term::mouse::CLICK: return TICKIT_TERM_MOUSEMODE_CLICK;
        case term::mouse::DRAG:  return TICKIT_TERM_MOUSEMODE_DRAG;
        case term::mouse::MOVE:  return TICKIT_TERM_MOUSEMODE_MOVE;
    }
    assert(false);
    return TICKIT_TERM_MOUSEMODE_OFF;
}

static inline TickitRect to_tickit(const rect& r)
{
    TickitRect result;
    tickit_rect_init_sized(&result, r.top(), r.left(), r.lines(), r.cols());
    return result;
}


template <int      _Event_type_code,
          typename _Ti_emitter_type,
          typename _Emitter_type,
          typename _Ti_event_info_type,
          typename _Event_type,
          typename _Ti_handler>
struct event_traits {
    static const TickitEventType event_code = static_cast<TickitEventType>(_Event_type_code);
    using ti_emitter                        = _Ti_emitter_type;
    using emitter                           = _Emitter_type;
    using ti_event_info                     = _Ti_event_info_type;
    using event_type                        = _Event_type;
    using handler                           = typename _Event_type::handler;
    using ti_handler                        = _Ti_handler;
};

using expose_event_traits = event_traits<TICKIT_EV_EXPOSE,
                                         TickitWindow,
                                         window,
                                         TickitExposeEventInfo,
                                         expose_event,
                                         TickitWindowEventFn>;

using focus_event_traits = event_traits<TICKIT_EV_FOCUS,
                                        TickitWindow,
                                        window,
                                        TickitFocusEventInfo,
                                        focus_event,
                                        TickitWindowEventFn>;

using geometry_change_event_traits = event_traits<TICKIT_EV_GEOMCHANGE,
                                                  TickitWindow,
                                                  window,
                                                  TickitGeomchangeEventInfo,
                                                  geometry_change_event,
                                                  TickitWindowEventFn>;

using key_event_traits = event_traits<TICKIT_EV_KEY,
                                      TickitWindow,
                                      window,
                                      TickitKeyEventInfo,
                                      key_event,
                                      TickitWindowEventFn>;

using mouse_event_traits = event_traits<TICKIT_EV_MOUSE,
                                        TickitWindow,
                                        window,
                                        TickitMouseEventInfo,
                                        mouse_event,
                                        TickitWindowEventFn>;

struct event_handler_info_base {
    virtual TickitEventType event_type() const = 0;
    virtual ~event_handler_info_base() {}
};

template <typename _Traits>
struct event_handler_info : public event_handler_info_base {
    event_handler_info(typename _Traits::emitter& e,
                       typename _Traits::handler& h)
        : emitter(e), handler(h) { }

    typename _Traits::emitter& emitter;
    typename _Traits::handler  handler;

    virtual TickitEventType event_type() const override {
        return _Traits::event_code;
    }

    void run(typename _Traits::ti_event_info* info);

    static int callback(typename _Traits::ti_emitter* ti_emitter, TickitEventType ev, void* info, void* user) {
        auto ti_event_info = reinterpret_cast<typename _Traits::ti_event_info*>(info);
        auto handler_info = reinterpret_cast<event_handler_info<_Traits>*>(user);
        assert(ev == _Traits::event_code);
        assert(ti_emitter == handler_info->emitter.unwrap());
        assert(handler_info->handler);
        handler_info->run(ti_event_info);
        return 0;
    }

    static int unbind_callback(typename _Traits::ti_emitter* ti_emitter, TickitEventType ev, void*, void* user) {
        assert(ev & TICKIT_EV_UNBIND);
        delete reinterpret_cast<event_handler_info<_Traits>*>(user);
        return 0;
    }

    using bind_func = int (*)(typename _Traits::ti_emitter*,
                              TickitEventType,
                              TickitBindFlags,
                              typename _Traits::ti_handler,
                              void*);

    template <bind_func _Bind>
    typename _Traits::emitter::binding bind() {
        int unbind_id = _Bind(emitter.unwrap(),
                              TICKIT_EV_UNBIND,
                              static_cast<TickitBindFlags>(0),
                              event_handler_info<_Traits>::unbind_callback,
                              this);
        int event_id = _Bind(emitter.unwrap(),
                             _Traits::event_code,
                             static_cast<TickitBindFlags>(0),
                             event_handler_info<_Traits>::callback,
                             this);
        return { event_id, unbind_id };
    }
};

/*
 * Specializations of event_handler_info<>::run()
 */
template <>
void event_handler_info<expose_event_traits>::run(TickitExposeEventInfo *info)
{
    rect r(info->rect.top, info->rect.left, info->rect.lines, info->rect.cols);
    render_buffer rb(render_buffer::NO_DELETE, info->rb);
    expose_event event { emitter, rb, r };
    handler(event);
}

template <>
void event_handler_info<focus_event_traits>::run(TickitFocusEventInfo *info)
{
    const enum focus_event::direction dir = (info->type == TICKIT_FOCUSEV_OUT)
        ? focus_event::direction::OUT
        : focus_event::direction::IN;

    if (emitter.unwrap() == info->win) {
        // The window which emitted the event is the one being focused.
        focus_event event { emitter, emitter, dir  };
        handler(event);
    } else {
        // A child window of the one emitting the event is focused: create
        // a temporary wrapper for the child window.
        window target(window::NO_DELETE, info->win);
        focus_event event { emitter, target, dir };
        handler(event);
    }
}

template <>
void event_handler_info<geometry_change_event_traits>::run(TickitGeomchangeEventInfo *info)
{
    rect old_geometry(info->oldrect.top,
                      info->oldrect.left,
                      info->oldrect.lines,
                      info->oldrect.cols);
    rect new_geometry(info->rect.top,
                      info->rect.left,
                      info->rect.lines,
                      info->rect.cols);
    geometry_change_event event { emitter, old_geometry, new_geometry };
    handler(event);
}

template <>
void event_handler_info<key_event_traits>::run(TickitKeyEventInfo *info)
{
    key_event event {
        emitter,
        (info->type == TICKIT_KEYEV_KEY) ? key_event::KEY : key_event::TEXT,
        static_cast<key_event::mod>(info->mod),
        info->str
    };
    handler(event);
}

template <>
void event_handler_info<mouse_event_traits>::run(TickitMouseEventInfo *info)
{
    mouse_event event {
        emitter,
        info->button,
        static_cast<enum mouse_event::type>(info->type),
        static_cast<mouse_event::mod>(info->mod),
        size_of_int(info->line),
        size_of_int(info->col)
    };
    handler(event);
}

using expose_handler_info          = event_handler_info<expose_event_traits>;
using focus_handler_info           = event_handler_info<focus_event_traits>;
using geometry_change_handler_info = event_handler_info<geometry_change_event_traits>;
using key_handler_info             = event_handler_info<key_event_traits>;
using mouse_handler_info           = event_handler_info<mouse_event_traits>;


rect::rect(rect::bounded_guard, int top, int left, int bottom, int right)
    : rect()
{
    tickit_rect_init_bounded(reinterpret_cast<TickitRect*>(this),
                             top, left, bottom, right);
}

int rect::bottom() const
{
    return tickit_rect_bottom(reinterpret_cast<const TickitRect*>(this));
}

int rect::right() const
{
    return tickit_rect_right(reinterpret_cast<const TickitRect*>(this));
}

bool rect::contains(const rect& r) const
{
    return tickit_rect_contains(reinterpret_cast<const TickitRect*>(this),
                                reinterpret_cast<const TickitRect*>(&r));
}

bool rect::intersects(const rect& r) const
{
    return tickit_rect_intersects(reinterpret_cast<const TickitRect*>(this),
                                  reinterpret_cast<const TickitRect*>(&r));
}

optional<const rect> rect::intersect(const rect& r) const
{
    rect result;
    if (tickit_rect_intersect(reinterpret_cast<TickitRect*>(&result),
                              reinterpret_cast<const TickitRect*>(this),
                              reinterpret_cast<const TickitRect*>(&r)))
        return optional<const rect>(result);
    return {};
}

const rect rect::translate(int downward, int rightward) const
{
    rect result(*this);  // Use the copy-constructor.
    tickit_rect_translate(reinterpret_cast<TickitRect*>(&result),
                          downward,
                          rightward);
    return result;
}


rect_set::rect_set()
    : m_rectset(std::shared_ptr<TickitRectSet>(tickit_rectset_new(),
                                               tickit_rectset_destroy))
{
}

rect_set& rect_set::add(const rect& r)
{
    tickit_rectset_add(m_rectset.get(), reinterpret_cast<const TickitRect*>(&r));
    return *this;
}

rect_set& rect_set::subtract(const rect& r)
{
    tickit_rectset_subtract(m_rectset.get(), reinterpret_cast<const TickitRect*>(&r));
    return *this;
}

rect_set& rect_set::clear()
{
    tickit_rectset_clear(m_rectset.get());
    return *this;
}

rect_set& rect_set::translate(int downward, int rightward)
{
    tickit_rectset_translate(m_rectset.get(), downward, rightward);
    return *this;
}

size_t rect_set::size() const
{
    return size_of_int(tickit_rectset_rects(m_rectset.get()));
}

std::vector<rect> rect_set::rects() const
{
    std::vector<rect> rects(size(), rect(0, 0, 0, 0));
    auto n = tickit_rectset_get_rects(m_rectset.get(),
                                      reinterpret_cast<TickitRect*>(rects.data()),
                                      rects.size());
    assert(n >= 0);
    assert(n == rects.size());
    return rects;
}

bool rect_set::contains(const rect& r) const
{
    return tickit_rectset_contains(m_rectset.get(),
                                   reinterpret_cast<const TickitRect*>(&r));
}

bool rect_set::intersects(const rect& r) const
{
    return tickit_rectset_intersects(m_rectset.get(),
                                     reinterpret_cast<const TickitRect*>(&r));
}


const pen::attr_reg pen::bold      = { pen::attr::BOLD,      1 };
const pen::attr_reg pen::underline = { pen::attr::UNDERLINE, 1 };
const pen::attr_reg pen::italic    = { pen::attr::ITALIC,    1 };
const pen::attr_reg pen::blink     = { pen::attr::BLINK,     1 };
const pen::attr_reg pen::reverse   = { pen::attr::REVERSE,   1 };
const pen::attr_reg pen::strike    = { pen::attr::STRIKE,    1 };


pen::pen(std::initializer_list<pen::attr_reg> l)
    : m_pen(tickit_pen_new())
{
    for (auto reg : l) set(reg);
}

pen::pen(const pen& other)
    : m_pen(tickit_pen_ref(other.m_pen))
{
}

pen::pen(TickitPen *p)
    : m_pen(tickit_pen_ref(p))
{
}

pen::~pen()
{
    tickit_pen_unref(m_pen);
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

bool pen::operator==(const pen& other) const
{
    return this == &other
        || m_pen == other.m_pen
        || tickit_pen_equiv(m_pen, other.m_pen);
}

bool pen::is_empty() const
{
    return !tickit_pen_is_nonempty(m_pen);
}

bool pen::is_non_default() const
{
    return tickit_pen_is_nondefault(m_pen);
}

bool pen::has(pen::attr tag, pen::has_mode mode) const
{
    return (mode = pen::NON_DEFAULT)
        ? tickit_pen_nondefault_attr(m_pen, to_tickit(tag))
        : tickit_pen_has_attr(m_pen, to_tickit(tag));
}

pen& pen::clear(pen::attr tag)
{
    tickit_pen_clear_attr(m_pen, to_tickit(tag));
    return *this;
}

pen& pen::clear()
{
    tickit_pen_clear(m_pen);
    return *this;
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

template<> bool pen::get<bool>(pen::attr tag) const
{
    TickitPenAttr attr_tag = to_tickit(tag);
    assert(tickit_pen_attrtype(attr_tag) ==  TICKIT_PENTYPE_BOOL);
    return tickit_pen_get_bool_attr(m_pen, attr_tag);
}

template<> int pen::get<int>(pen::attr tag) const
{
    TickitPenAttr attr_tag = to_tickit(tag);
    switch (tickit_pen_attrtype(attr_tag)) {
        case TICKIT_PENTYPE_INT:
            return tickit_pen_get_int_attr(m_pen, attr_tag);
        case TICKIT_PENTYPE_COLOUR:
            return tickit_pen_get_colour_attr(m_pen, attr_tag);
        default:
            assert(false);
            return -1;
    }
}

pen& pen::copy_from(const pen& other, pen::copy_mode mode)
{
    tickit_pen_copy(m_pen, other.m_pen, mode == pen::copy_mode::OVERWRITE);
    return *this;
}


render_buffer::render_buffer(size_t lines, size_t cols)
    : m_renderbuffer(std::shared_ptr<TickitRenderBuffer>(tickit_renderbuffer_new(int_of_size(lines),
                                                                                 int_of_size(cols)),
                                                         tickit_renderbuffer_destroy))
{
}

render_buffer& render_buffer::reset()
{
    tickit_renderbuffer_reset(m_renderbuffer.get());
    return *this;
}

render_buffer& render_buffer::save()
{
    tickit_renderbuffer_save(m_renderbuffer.get());
    return *this;
}

render_buffer& render_buffer::save_pen()
{
    tickit_renderbuffer_savepen(m_renderbuffer.get());
    return *this;
}

render_buffer& render_buffer::restore()
{
    tickit_renderbuffer_restore(m_renderbuffer.get());
    return *this;
}

std::pair<size_t, size_t> render_buffer::size() const
{
    int lines, cols;
    tickit_renderbuffer_get_size(m_renderbuffer.get(), &lines, &cols);
    return { size_of_int(lines), size_of_int(cols) };
}

render_buffer& render_buffer::translate(int downward, int rightward)
{
    tickit_renderbuffer_translate(m_renderbuffer.get(), downward, rightward);
    return *this;
}

render_buffer& render_buffer::clip(const rect& r)
{
    // TODO: Check whether a copy of the rectangle is really needed.
    TickitRect r_copy(to_tickit(r));
    tickit_renderbuffer_clip(m_renderbuffer.get(), &r_copy);
    return *this;
}

render_buffer& render_buffer::mask(const rect& r)
{
    // TODO: Check whether a copy of the rectangle is really needed.
    TickitRect r_copy(to_tickit(r));
    tickit_renderbuffer_mask(m_renderbuffer.get(), &r_copy);
    return *this;
}

render_buffer& render_buffer::set_pen(const pen& pen)
{
    tickit_renderbuffer_setpen(m_renderbuffer.get(), pen.m_pen);
    return *this;
}

render_buffer& render_buffer::goto_position(int line, int col)
{
    tickit_renderbuffer_goto(m_renderbuffer.get(), line, col);
    return *this;
}

render_buffer& render_buffer::clear_position()
{
    tickit_renderbuffer_ungoto(m_renderbuffer.get());
    return *this;
}

optional<std::pair<size_t, size_t>> render_buffer::get_position() const
{
    if (tickit_renderbuffer_has_cursorpos(m_renderbuffer.get())) {
        int lines, cols;
        tickit_renderbuffer_get_cursorpos(m_renderbuffer.get(), &lines, &cols);
        return optional<std::pair<size_t, size_t>>({ lines, cols });
    }
    return optional<std::pair<size_t, size_t>>();
}

render_buffer& render_buffer::skip(size_t cols)
{
    tickit_renderbuffer_skip(m_renderbuffer.get(),
                             int_of_size(cols));
    return *this;
}

render_buffer& render_buffer::skip_to(size_t col)
{
    tickit_renderbuffer_skip_to(m_renderbuffer.get(),
                                int_of_size(col));
    return *this;
}

render_buffer& render_buffer::skip_at(size_t line, size_t col, size_t cols)
{
    tickit_renderbuffer_skip_at(m_renderbuffer.get(),
                                int_of_size(line),
                                int_of_size(col),
                                int_of_size(cols));
    return *this;
}

render_buffer& render_buffer::text(const std::string& s)
{
    tickit_renderbuffer_textn(m_renderbuffer.get(), s.data(), s.size());
    return *this;
}

render_buffer& render_buffer::text(const std::string& s, size_t len)
{
    tickit_renderbuffer_textn(m_renderbuffer.get(), s.data(), len);
    return *this;
}

render_buffer& render_buffer::text_at(size_t line, size_t col, const std::string& s)
{
    tickit_renderbuffer_textn_at(m_renderbuffer.get(),
                                 int_of_size(line),
                                 int_of_size(col),
                                 s.data(),
                                 s.size());
    return *this;
}

render_buffer& render_buffer::text_at(size_t line, size_t col, const std::string& s, size_t len)
{
    tickit_renderbuffer_textn_at(m_renderbuffer.get(),
                                 int_of_size(line),
                                 int_of_size(col),
                                 s.data(),
                                 len);
    return *this;
}

render_buffer& render_buffer::unichar(unsigned long codepoint)
{
    tickit_renderbuffer_char(m_renderbuffer.get(),
                             static_cast<long>(codepoint));
    return *this;
}

render_buffer& render_buffer::unichar_at(size_t line, size_t col, unsigned long codepoint)
{
    tickit_renderbuffer_char_at(m_renderbuffer.get(),
                                int_of_size(line),
                                int_of_size(col),
                                static_cast<long>(codepoint));
    return *this;
}

render_buffer& render_buffer::hline_at(size_t line, size_t startcol, size_t endcol,
                                               render_buffer::line_style style,
                                               render_buffer::line_caps caps)
{
    tickit_renderbuffer_hline_at(m_renderbuffer.get(),
                                 int_of_size(line),
                                 int_of_size(startcol),
                                 int_of_size(endcol),
                                 to_tickit(style),
                                 to_tickit(caps));
    return *this;
}

render_buffer& render_buffer::vline_at(size_t startline, size_t endline, size_t col,
                                               render_buffer::line_style style,
                                               render_buffer::line_caps caps)
{
    tickit_renderbuffer_vline_at(m_renderbuffer.get(),
                                 int_of_size(startline),
                                 int_of_size(endline),
                                 int_of_size(col),
                                 to_tickit(style),
                                 to_tickit(caps));
    return *this;
}

render_buffer& render_buffer::erase(size_t cols)
{
    tickit_renderbuffer_erase(m_renderbuffer.get(),
                              int_of_size(cols));
    return *this;
}

render_buffer& render_buffer::erase_to(size_t col)
{
    tickit_renderbuffer_erase_to(m_renderbuffer.get(),
                                 int_of_size(col));
    return *this;
}

render_buffer& render_buffer::erase_at(size_t line, size_t col, size_t cols)
{
    tickit_renderbuffer_erase_at(m_renderbuffer.get(),
                                 int_of_size(line),
                                 int_of_size(col),
                                 int_of_size(cols));
    return *this;
}

render_buffer& render_buffer::erase(const rect& r)
{
    // TODO: Check whether a copy of the rectangle is really needed.
    TickitRect r_copy(to_tickit(r));
    tickit_renderbuffer_eraserect(m_renderbuffer.get(), &r_copy);
    return *this;
}

render_buffer& render_buffer::erase()
{
    tickit_renderbuffer_clear(m_renderbuffer.get());
    return *this;
}

render_buffer& render_buffer::blit(const render_buffer& source)
{
    tickit_renderbuffer_blit(m_renderbuffer.get(),
                             source.m_renderbuffer.get());
    return *this;
}


term::term()
    : m_term(std::shared_ptr<TickitTerm>(tickit_term_new(),
                                         tickit_term_destroy))
{
}

term::term(term::stdio_guard)
    : m_term(std::shared_ptr<TickitTerm>(tickit_term_open_stdio(),
                                         tickit_term_destroy))
{
}

term::term(const std::string& term_type)
    : m_term(std::shared_ptr<TickitTerm>(tickit_term_new_for_termtype(term_type.c_str()),
                                         tickit_term_destroy))
{
}

std::string term::term_type() const
{
    return std::string(tickit_term_get_termtype(m_term.get()));
}

term& term::set_pen(const pen& p, term::set_pen_mode mode)
{
    if (mode == term::SET_PEN_NORMAL) {
        tickit_term_setpen(m_term.get(), p.m_pen);
    } else {
        tickit_term_chpen(m_term.get(), p.m_pen);
    }
    return *this;
}

term& term::flush()
{
    tickit_term_flush(m_term.get());
    return *this;
}

term& term::text(const std::string& s, size_t len)
{
    tickit_term_printn(m_term.get(), s.data(), len);
    return *this;
}

term& term::text(const std::string& s)
{
    tickit_term_printn(m_term.get(), s.data(), s.size());
    return *this;
}

term& term::clear()
{
    tickit_term_clear(m_term.get());
    return *this;
}

term& term::blit(const render_buffer& rb)
{
    tickit_renderbuffer_flush_to_term(rb.m_renderbuffer.get(), m_term.get());
    return *this;
}

term& term::wait_ready(unsigned long msec)
{
    tickit_term_await_started_msec(m_term.get(), msec);
    return *this;
}

term& term::wait_input(long msec)
{
    tickit_term_input_wait_msec(m_term.get(), msec);
    return *this;
}

term& term::ctl(enum term::mouse mode)
{
    tickit_term_setctl_int(m_term.get(), TICKIT_TERMCTL_MOUSE, to_tickit(mode));
    return *this;
}

term& term::ctl(enum term::screen mode)
{
    tickit_term_setctl_int(m_term.get(),
                           TICKIT_TERMCTL_ALTSCREEN,
                           (mode == term::screen::ALT) ? 1 : 0);
    return *this;
}


window::window(window& parent, const rect& r, window::flags f)
    : m_window(std::shared_ptr<TickitWindow>(tickit_window_new(parent.m_window.get(),
                                                               to_tickit(r),
                                                               to_tickit(f)),
                                             tickit_window_destroy))
{
}

window::window(term& t)
    : m_window(std::shared_ptr<TickitWindow>(tickit_window_new_root(t.m_term.get()),
                                             tickit_window_destroy))
{
}

window::window(const window& w)
    : m_window(w.m_window)
{
}

window window::root() const
{
    return window(NO_DELETE, tickit_window_root(m_window.get()));
}

optional<window> window::parent() const
{
    if (auto w = tickit_window_parent(m_window.get()))
        return optional<window>(window(NO_DELETE, w));
    return optional<window>();
}

size_t window::top() const
{
    return size_of_int(tickit_window_top(m_window.get()));
}

size_t window::left() const
{
    return size_of_int(tickit_window_left(m_window.get()));
}

size_t window::lines() const
{
    return size_of_int(tickit_window_lines(m_window.get()));
}

size_t window::cols() const
{
    return size_of_int(tickit_window_cols(m_window.get()));
}

size_t window::bottom() const
{
    return size_of_int(tickit_window_bottom(m_window.get()));
}

size_t window::right() const
{
    return size_of_int(tickit_window_right(m_window.get()));
}

const rect window::abs_geometry() const
{
    TickitRect r(tickit_window_get_geometry(m_window.get()));
    return rect(r.top, r.left, r.lines, r.cols);
}

const rect window::geometry() const
{
    TickitRect r(tickit_window_get_geometry(m_window.get()));
    return rect(r.top, r.left, r.lines, r.cols);
}

window& window::set_geometry(const rect &r)
{
    tickit_window_set_geometry(m_window.get(), to_tickit(r));
    return *this;
}

window& window::resize(size_t lines, size_t cols)
{
    tickit_window_resize(m_window.get(),
                         int_of_size(lines),
                         int_of_size(cols));
    return *this;
}

window& window::reposition(size_t top, size_t left)
{
    tickit_window_reposition(m_window.get(),
                             int_of_size(top),
                             int_of_size(left));
    return *this;
}

window& window::raise()
{
    tickit_window_raise(m_window.get());
    return *this;
}

window& window::raise_to_front()
{
    tickit_window_raise_to_front(m_window.get());
    return *this;
}

window& window::lower()
{
    tickit_window_lower(m_window.get());
    return *this;
}

window& window::lower_to_back()
{
    tickit_window_lower_to_back(m_window.get());
    return *this;
}

bool window::is_focused() const
{
    return tickit_window_is_focused(m_window.get());
}

window& window::focus()
{
    tickit_window_take_focus(m_window.get());
    return *this;
}

bool window::is_visible() const
{
    return tickit_window_is_visible(m_window.get());
}

window& window::show()
{
    tickit_window_show(m_window.get());
    return *this;
}

window& window::hide()
{
    tickit_window_hide(m_window.get());
    return *this;
}

window& window::expose(const rect& r)
{
    tickit_window_expose(m_window.get(),
                         reinterpret_cast<const TickitRect*>(&r));
    return *this;
}

window& window::expose()
{
    tickit_window_expose(m_window.get(), nullptr);
    return *this;
}

window& window::flush()
{
    tickit_window_flush(m_window.get());
    return *this;
}

struct pen window::pen() const
{
    return ti::pen(tickit_window_get_pen(m_window.get()));
}

window& window::set_pen(struct pen& p)
{
    tickit_window_set_pen(m_window.get(), p.m_pen);
    return *this;
}

window& window::scroll(int downward, int rightward, window::scroll_with_children_guard guard)
{
    assert(guard == window::scroll_with_children_guard::WITH_CHILDREN);
    tickit_window_scroll_with_children(m_window.get(), downward, rightward);
    return *this;
}

window& window::scroll(int downward, int rightward)
{
    tickit_window_scroll(m_window.get(), downward, rightward);
    return *this;
}

window& window::scroll(const rect& r, int downward, int rightward)
{
    tickit_window_scrollrect(m_window.get(),
                             reinterpret_cast<const TickitRect*>(&r),
                             downward,
                             rightward,
                             tickit_window_get_pen(m_window.get()));
    return *this;
}

window& window::scroll(const rect& r, int downward, int rightward, struct pen& p)
{
    tickit_window_scrollrect(m_window.get(),
                             reinterpret_cast<const TickitRect*>(&r),
                             downward,
                             rightward,
                             p.m_pen);
    return *this;
}

window& window::goto_position(size_t line, size_t col)
{
    tickit_window_set_cursor_position(m_window.get(),
                                      int_of_size(line),
                                      int_of_size(col));
    return *this;
}

window& window::set_cursor_shape(window::cursor_shape shape)
{
    tickit_window_set_cursor_shape(m_window.get(), to_tickit(shape));
    return *this;
}

window& window::set_cursor_visible(bool visible)
{
    tickit_window_set_cursor_visible(m_window.get(), visible);
    return *this;
}

window::binding window::bind_expose(expose_event::handler f)
{
    auto handler_info = new expose_handler_info(*this, f);
    return handler_info->bind<tickit_window_bind_event>();
}

window::binding window::bind_focus(focus_event::handler f)
{
    auto handler_info = new focus_handler_info(*this, f);
    return handler_info->bind<tickit_window_bind_event>();
}

window::binding window::bind_geometry_change(geometry_change_event::handler f)
{
    auto handler_info = new geometry_change_handler_info(*this, f);
    return handler_info->bind<tickit_window_bind_event>();
}

window::binding window::bind_key(key_event::handler f)
{
    auto handler_info = new key_handler_info(*this, f);
    return handler_info->bind<tickit_window_bind_event>();
}

window::binding window::bind_mouse(mouse_event::handler f)
{
    auto handler_info = new mouse_handler_info(*this, f);
    return handler_info->bind<tickit_window_bind_event>();
}

window& window::unbind(const window::binding& b)
{
    // Unbind the actual handler first, so our unbind event handler
    // has the chance of deleting the handler_info<> instance.
    tickit_window_unbind_event_id(m_window.get(), b.m_id);
    tickit_window_unbind_event_id(m_window.get(), b.m_unbind_id);
    return *this;
}


} // namespace ti
