require 'gcprof'

=begin
GC::Profiler.enable
at_exit{ GC::Profiler.raw_data.map{ |d| p(d) } }
=end

prev_alloc = GC.stat(:total_allocated_object)
GCProf.after_gc_hook = proc { |info, start, end_mark, end_sweep|
  before = start
  after = end_sweep || end_mark

  if !info[:immediate_sweep] && end_sweep
    STDERR.printf "[%f:% 4d]                   swept after %1.3fs  swept:% 8d\n",
      after[:time],
      after[:count],
      after[:time] - end_mark[:time],
      after[:heap_swept_slot]
  else
    type = info[:major_by] ? "MAJOR GC" : "minor gc"
    diff = after[:time] - before[:time]
    diff_alloc, prev_alloc = before[:total_allocated_object] - prev_alloc, after[:total_allocated_object]

    STDERR.printf "[%f:% 4d] %s (% 13s) took %1.3fs  alloc:% 8d  live: % 8d -> % 8d  free: % 8d -> % 8d %s\n",
      before[:time],
      before[:count],
      type,
      info.values_at(:major_by, :gc_by).compact.join(","),
      after[:time] - before[:time],
      diff_alloc,
      before[:heap_live_slot],
      after[:heap_live_slot],
      before[:heap_free_slot],
      after[:heap_free_slot],
      info[:immediate_sweep] ? "  +immediate_sweep" : ""
  end
}
