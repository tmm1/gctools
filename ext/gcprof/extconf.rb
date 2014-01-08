require 'mkmf'
have_func('rb_gc_stat')
have_func('rb_gc_latest_gc_info')
have_const('RUBY_INTERNAL_EVENT_GC_END_MARK')
create_makefile('gctools/gcprof')
