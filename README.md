## gctools

gc profiler/logger and oobgc for rgengc in ruby 2.1

### design

built on new apis and events offered in ruby 2.1:

  * rb_gc_stat()
  * rb_gc_latest_gc_info()
  * RUBY_INTERNAL_EVENT_GC_START
  * RUBY_INTERNAL_EVENT_GC_END_MARK
  * RUBY_INTERNAL_EVENT_GC_END_SWEEP

### usage

require 'gctools/logger'
