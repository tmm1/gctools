require 'mkmf'
have_func('rb_gc_stat')
have_func('rb_gc_latest_gc_info')
gc_event = have_const('RUBY_INTERNAL_EVENT_GC_END_MARK')

if gc_event
  create_makefile('gctools/gcprof')
else
  File.open('Makefile', 'w') do |f|
    f.puts "install:\n\t\n"
  end
end
