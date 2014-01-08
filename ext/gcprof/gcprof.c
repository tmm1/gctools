#include <time.h>
#include <sys/time.h>
#include "ruby/ruby.h"
#include "ruby/debug.h"

static VALUE mGCProf;
static VALUE sym_time, sym_count, sym_immediate_sweep;
static struct {
    size_t serial;
    VALUE tpval;
    VALUE proc;

    VALUE info;
    VALUE start;
    VALUE end_mark;
    VALUE end_sweep;
} _gcprof;

  static inline double
walltime()
{
  struct timespec ts;
#ifdef HAVE_CLOCK_GETTIME
  if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
    rb_sys_fail("clock_gettime");
  }
#else
  {
    struct timeval tv;
    if (gettimeofday(&tv, 0) < 0) {
      rb_sys_fail("gettimeofday");
    }
    ts.tv_sec = tv.tv_sec;
    ts.tv_nsec = tv.tv_usec * 1000;
  }
#endif
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static const char *
event_flag_name(rb_event_flag_t flag)
{
  switch (flag) {
    case RUBY_INTERNAL_EVENT_GC_START:
      return "gc_start";
    case RUBY_INTERNAL_EVENT_GC_END_MARK:
      return "gc_end_mark";
    case RUBY_INTERNAL_EVENT_GC_END_SWEEP:
      return "gc_end_sweep";
    default:
      return "unknown";
  }
}

static void
invoke_proc(void *data)
{
  VALUE args[4] = {_gcprof.info, _gcprof.start, _gcprof.end_mark, _gcprof.end_sweep};
  if (_gcprof.serial != (size_t)data) return;
  if (_gcprof.serial != NUM2SIZET(rb_hash_aref(_gcprof.end_sweep, sym_count))) args[3] = Qnil;
  rb_proc_call_with_block(_gcprof.proc, 4, args, Qnil);
}

static void
gc_hook_i(VALUE tpval, void *data)
{
  rb_trace_arg_t *tparg = rb_tracearg_from_tracepoint(tpval);
  rb_event_flag_t flag = rb_tracearg_event_flag(tparg);
  VALUE obj =
    flag == RUBY_INTERNAL_EVENT_GC_START ? _gcprof.start :
    flag == RUBY_INTERNAL_EVENT_GC_END_MARK ? _gcprof.end_mark :
    _gcprof.end_sweep;

  if (0) {
    fprintf(stderr, "TRACEPOINT: time=%f, event=%s(%zu)\n", walltime(), event_flag_name(rb_tracearg_event_flag(tparg)), rb_gc_count());
  }

  rb_hash_aset(obj, sym_time, DBL2NUM(walltime()));
  rb_gc_stat(obj);

  switch (flag) {
    case RUBY_INTERNAL_EVENT_GC_START:
      rb_gc_latest_gc_info(_gcprof.info);
      _gcprof.serial = rb_gc_count();
      return;
    case RUBY_INTERNAL_EVENT_GC_END_MARK:
      if (RTEST(rb_hash_aref(_gcprof.info, sym_immediate_sweep))) return;
      /* fall through */
    case RUBY_INTERNAL_EVENT_GC_END_SWEEP:
      rb_postponed_job_register(0, invoke_proc, (void *)_gcprof.serial);
      break;
  }
}

static VALUE
set_after_gc_hook(VALUE module, VALUE proc)
{
  rb_event_flag_t events;

  /* disable previous keys */
  if (RTEST(_gcprof.tpval)) {
    rb_tracepoint_disable(_gcprof.tpval);
    _gcprof.tpval = Qnil;
    _gcprof.proc = Qnil;
  }

  if (RTEST(proc)) {
    if (!rb_obj_is_proc(proc)) {
      rb_raise(rb_eTypeError, "trace_func needs to be Proc");
    }

    events = RUBY_INTERNAL_EVENT_GC_START|RUBY_INTERNAL_EVENT_GC_END_MARK|RUBY_INTERNAL_EVENT_GC_END_SWEEP;
    _gcprof.tpval = rb_tracepoint_new(0, events, gc_hook_i, (void *)0);
    _gcprof.proc = proc;
    rb_tracepoint_enable(_gcprof.tpval);
  }

  return proc;
}


void
Init_gcprof(void)
{
  mGCProf = rb_define_module("GCProf");
  rb_define_module_function(mGCProf, "after_gc_hook=", set_after_gc_hook, 1);

  sym_immediate_sweep = ID2SYM(rb_intern("immediate_sweep"));
  sym_time = ID2SYM(rb_intern("time"));
  sym_count = ID2SYM(rb_intern("count"));

  rb_gc_latest_gc_info(_gcprof.info = rb_hash_new());
  _gcprof.start = rb_hash_new();
  rb_hash_aset(_gcprof.start, sym_time, INT2FIX(0));
  rb_gc_stat(_gcprof.start);
  _gcprof.end_mark = rb_obj_dup(_gcprof.start);
  _gcprof.end_sweep = rb_obj_dup(_gcprof.start);

  rb_gc_register_mark_object(_gcprof.info);
  rb_gc_register_mark_object(_gcprof.start);
  rb_gc_register_mark_object(_gcprof.end_mark);
  rb_gc_register_mark_object(_gcprof.end_sweep);

  rb_gc_register_address(&_gcprof.proc);
  rb_gc_register_address(&_gcprof.tpval);
}
