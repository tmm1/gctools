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
} _oobgc;

static VALUE mOOB;
static VALUE id_total_allocated_object, id_heap_swept_slot, id_heap_tomb_page_length, id_heap_final_slot;

static void
gc_event_i(VALUE tpval, void *data)
{
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  rb_event_flag_t flag = rb_tracearg_event_flag(tparg);

  switch (flag) {
    case RUBY_INTERNAL_EVENT_GC_START:
      _oobgc.allocation_limit = 0;
      _oobgc.start.total_allocated_object = rb_gc_stat(id_total_allocated_object);
      _oobgc.start.heap_tomb_page_length = rb_gc_stat(id_heap_tomb_page_length);
      break;

    case RUBY_INTERNAL_EVENT_GC_END_MARK:
      _oobgc.sweep_needed = 1;
      break;

    case RUBY_INTERNAL_EVENT_GC_END_SWEEP:
      _oobgc.sweep_needed = 0;
      _oobgc.allocation_limit =
        _oobgc.start.total_allocated_object +
        rb_gc_stat(id_heap_swept_slot) +
        (_oobgc.start.heap_tomb_page_length * _oobgc.heap_obj_limit) -
        rb_gc_stat(id_heap_final_slot);
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

static VALUE
oobgc(VALUE self)
{
  size_t curr = rb_gc_stat(id_total_allocated_object);
  if (!_oobgc.installed) install();

  if (!_oobgc.prev_allocated_object) {
    _oobgc.prev_allocated_object = curr;
  } else {
    size_t diff = curr - _oobgc.prev_allocated_object;
    _oobgc.prev_allocated_object = curr;

    if (_oobgc.threshold.mean)
      _oobgc.threshold.mean = (diff * 2 / 3) + (_oobgc.threshold.mean / 3);
    else
      _oobgc.threshold.mean = diff;

    if (diff > _oobgc.threshold.max)
      _oobgc.threshold.max = diff;
  }

  if (_oobgc.sweep_needed) {
    /* lazy sweep started sometime recently.
     * disable/enable the GC to force gc_rest_sweep() OOB
     */
    if (rb_gc_disable() == Qfalse) rb_gc_enable();
    return Qtrue;
  } else {
    if (curr >= _oobgc.allocation_limit - _oobgc.threshold.max) {
      rb_gc_start();
      return Qtrue;
    }
  }

  return Qfalse;
}

static VALUE
gcstat(VALUE self, VALUE key)
{
  if (!_oobgc.installed)
    return Qnil;

  return key;
}

void
Init_oobgc()
{
  mOOB = rb_define_module_under(rb_mGC, "OOB");
  rb_define_singleton_method(mOOB, "setup", install, 0);
  rb_define_singleton_method(mOOB, "run", oobgc, 0);
  rb_define_singleton_method(mOOB, "stat", gcstat, 1);

#define S(name) id_##name = ID2SYM(rb_intern(#name));
  S(total_allocated_object);
  S(heap_swept_slot);
  S(heap_tomb_page_length);
  S(heap_final_slot);
#undef S

  _oobgc.heap_obj_limit =
    rb_hash_aref(rb_const_get(rb_mGC, rb_intern("INTERNAL_CONSTANTS")), ID2SYM(rb_intern("HEAP_OBJ_LIMIT")));;
}
