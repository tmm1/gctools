#include <time.h>
#include <sys/time.h>
#include "ruby/ruby.h"
#include "ruby/intern.h"
#include "ruby/debug.h"

struct {
  int installed;
  VALUE tpval;

  int sweep_needed;
  size_t prev_allocated_object;
  size_t allocation_limit;
  size_t heap_obj_limit;

  struct {
    size_t total_allocated_object;
    size_t heap_tomb_page_length;
  } start;

  struct {
    size_t mean;
    size_t max;
  } threshold;

  struct {
    size_t minor;
    size_t major;
    size_t sweep;
  } stat;
} _oobgc;

static VALUE mOOB;
static VALUE sym_total_allocated_object, sym_heap_swept_slot, sym_heap_tomb_page_length, sym_heap_final_slot;
static VALUE sym_old_object, sym_old_object_limit, sym_remembered_shady_object, sym_remembered_shady_object_limit;
static VALUE sym_major_by, sym_count, sym_major_count, sym_minor_count, sym_sweep_count, minor_gc_args;
static    ID id_start;

static void
gc_event_i(VALUE tpval, void *data)
{
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  rb_event_flag_t flag = rb_tracearg_event_flag(tparg);

  switch (flag) {
    case RUBY_INTERNAL_EVENT_GC_START:
      _oobgc.allocation_limit = 0;
      _oobgc.start.total_allocated_object = rb_gc_stat(sym_total_allocated_object);
      _oobgc.start.heap_tomb_page_length = rb_gc_stat(sym_heap_tomb_page_length);
      break;

    case RUBY_INTERNAL_EVENT_GC_END_MARK:
      _oobgc.sweep_needed = 1;
      break;

    case RUBY_INTERNAL_EVENT_GC_END_SWEEP:
      _oobgc.sweep_needed = 0;
      _oobgc.allocation_limit =
        _oobgc.start.total_allocated_object +
        rb_gc_stat(sym_heap_swept_slot) +
        (rb_gc_stat(sym_heap_tomb_page_length) * _oobgc.heap_obj_limit) -
        rb_gc_stat(sym_heap_final_slot);
      break;
  }
}

static VALUE
install()
{
  rb_event_flag_t events =
    RUBY_INTERNAL_EVENT_GC_START    |
    RUBY_INTERNAL_EVENT_GC_END_MARK |
    RUBY_INTERNAL_EVENT_GC_END_SWEEP;

  if (_oobgc.installed)
    return Qfalse;

  if (!_oobgc.tpval) {
    _oobgc.tpval = rb_tracepoint_new(0, events, gc_event_i, (void *)0);
    rb_ivar_set(mOOB, rb_intern("tpval"), _oobgc.tpval);
  }

  rb_tracepoint_enable(_oobgc.tpval);
  _oobgc.installed = 1;
  return Qtrue;
}

static void
gc_start_major()
{
  rb_gc_start();
  _oobgc.stat.major++;
}

static void
gc_start_minor()
{
  rb_funcall(rb_mGC, id_start, 1, minor_gc_args);

  if (RTEST(rb_gc_latest_gc_info(sym_major_by)))
    _oobgc.stat.major++;
  else
    _oobgc.stat.minor++;
}

static VALUE
oobgc(VALUE self)
{
  size_t curr = rb_gc_stat(sym_total_allocated_object);
  if (!_oobgc.installed) install();

  if (!_oobgc.prev_allocated_object) {
    _oobgc.prev_allocated_object = curr;
  } else {
    size_t diff = curr - _oobgc.prev_allocated_object;
    _oobgc.prev_allocated_object = curr;

    if (_oobgc.threshold.mean)
      _oobgc.threshold.mean = (diff / 4) + (_oobgc.threshold.mean * 3 / 4);
    else
      _oobgc.threshold.mean = diff;

    if (diff > _oobgc.threshold.max)
      _oobgc.threshold.max = diff;
    if (_oobgc.threshold.max > 200000)
      _oobgc.threshold.max = 200000;
  }

  if (_oobgc.sweep_needed) {
    /* lazy sweep started sometime recently.
     * disable/enable the GC to force gc_rest_sweep() OOB
     */
    if (rb_gc_disable() == Qfalse) rb_gc_enable();
    _oobgc.stat.sweep++;
    return Qtrue;

  } else if (_oobgc.allocation_limit) {
    /* GC will be required when total_allocated_object gets
     * close to allocation_limit
     */
    if ((rb_gc_stat(sym_old_object) >= rb_gc_stat(sym_old_object_limit)*0.97 ||
        rb_gc_stat(sym_remembered_shady_object) >= rb_gc_stat(sym_remembered_shady_object_limit)*0.97) &&
        curr >= _oobgc.allocation_limit - _oobgc.threshold.max*0.98) {
      /*fprintf(stderr, "oobgc MAJOR: %zu >= %zu - %zu\n", curr, _oobgc.allocation_limit, _oobgc.threshold.max);*/
      gc_start_major();
      return Qtrue;

    } else if (curr >= _oobgc.allocation_limit - _oobgc.threshold.mean) {
      /*fprintf(stderr, "oobgc minor: %zu >= %zu - %zu\n", curr, _oobgc.allocation_limit, _oobgc.threshold.mean);*/
      gc_start_minor();
      return Qtrue;
    }
  }

  return Qfalse;
}

static VALUE
oobgc_stat(VALUE self, VALUE key)
{
  if (!_oobgc.installed)
    return Qnil;

  if (key == sym_count)
    return SIZET2NUM(_oobgc.stat.major + _oobgc.stat.minor + _oobgc.stat.sweep);
  else if (key == sym_major_count)
    return SIZET2NUM(_oobgc.stat.major);
  else if (key == sym_minor_count)
    return SIZET2NUM(_oobgc.stat.minor);
  else if (key == sym_sweep_count)
    return SIZET2NUM(_oobgc.stat.sweep);
  else
    return Qnil;
}

static VALUE
oobgc_clear(VALUE self)
{
  MEMZERO(&_oobgc.stat, _oobgc.stat, 1);
  return Qnil;
}

void
Init_oobgc()
{
  mOOB = rb_define_module_under(rb_mGC, "OOB");
  rb_autoload(mOOB, rb_intern_const("UnicornMiddleware"), "gctools/oobgc/unicorn_middleware.rb");
  rb_autoload(mOOB, rb_intern_const("PumaMiddleware"), "gctools/oobgc/puma_middleware.rb");

  rb_define_singleton_method(mOOB, "setup", install, 0);
  rb_define_singleton_method(mOOB, "run", oobgc, 0);
  rb_define_singleton_method(mOOB, "stat", oobgc_stat, 1);
  rb_define_singleton_method(mOOB, "clear", oobgc_clear, 0);

#define S(name) sym_##name = ID2SYM(rb_intern(#name));
  S(total_allocated_object);
  S(heap_swept_slot);
  S(heap_tomb_page_length);
  S(heap_final_slot);

  S(old_object);
  S(old_object_limit);
  S(remembered_shady_object);
  S(remembered_shady_object_limit);

  S(major_by);
  S(count);
  S(major_count);
  S(minor_count);
  S(sweep_count);
#undef S

  id_start = rb_intern("start");
  _oobgc.heap_obj_limit =
    NUM2SIZET(rb_hash_aref(rb_const_get(rb_mGC, rb_intern("INTERNAL_CONSTANTS")), ID2SYM(rb_intern("HEAP_OBJ_LIMIT"))));

  minor_gc_args = rb_hash_new();
  rb_hash_aset(minor_gc_args, ID2SYM(rb_intern("full_mark")), Qfalse);
  rb_ivar_set(mOOB, rb_intern("minor_gc_args"), minor_gc_args);
}
