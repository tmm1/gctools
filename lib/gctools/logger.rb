require 'gctools/gcprof'

=begin
GC::Profiler.enable
at_exit{ GC::Profiler.raw_data.map{ |d| p(d) } }
=end

=begin
prev_alloc = GC.stat(:total_allocated_object)
GCProf.after_gc_hook = proc { |info, start, end_mark, end_sweep|
  before = start
  after = end_sweep || end_mark

  if !info[:immediate_sweep] && end_sweep
    STDERR.printf "[%f:% 4d]                   swept after %1.3fs  swept:% 8d  marked:% 8d\n",
      after[:time],
      after[:count],
      after[:time] - end_mark[:time],
      after[:heap_swept_slot],
      marked_num
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
=end

GCProf.after_gc_hook =
proc{ |info, start, end_mark, end_sweep|
  if end_sweep
    # eden_slots = oldgen_slots + remembered_shady_slots + longlived_infant_slots + new_infant_slots + empty_eden_slots
    # heap_swept_slot = new_infant_slots + empty_eden_slots + empty_moved_tomb_slots
    eden_slots = GC::INTERNAL_CONSTANTS[:HEAP_OBJ_LIMIT] * end_sweep[:heap_eden_page_length]
    oldgen_slots = end_sweep[:old_object]
    remembered_shady_slots = end_sweep[:remembered_shady_object]
    new_infant_slots = end_sweep[:total_allocated_object] - end_mark[:total_allocated_object]
    empty_moved_tomb_slots = GC::INTERNAL_CONSTANTS[:HEAP_OBJ_LIMIT] * (end_sweep[:heap_tomb_page_length] - end_mark[:heap_tomb_page_length])
    empty_eden_slots = end_sweep[:heap_swept_slot] - new_infant_slots - empty_moved_tomb_slots
    longlived_infant_slots = eden_slots - oldgen_slots - remembered_shady_slots - new_infant_slots - empty_eden_slots

    # total_slots = eden_slots + tomb_slots
    tomb_slots = GC::INTERNAL_CONSTANTS[:HEAP_OBJ_LIMIT] * end_sweep[:heap_tomb_page_length]
    total_slots = eden_slots + tomb_slots

    STDERR.printf "[%s in %1.3fs (% 13s)] % 8d slots = old (%4.1f%%) + remembered_shady (%4.1f%%) + shady (%4.1f%%) + infant (%4.1f%%) + empty (%4.1f%%) + tomb (%4.1f%%)\n",
      info[:major_by] ? 'MAJOR GC' : 'minor gc',
      end_mark[:time] - start[:time],
      info.values_at(:major_by, :gc_by).compact.join(","),
      total_slots,
      oldgen_slots * 100.0 / total_slots,
      remembered_shady_slots * 100.0 / total_slots,
      longlived_infant_slots * 100.0 / total_slots,
      new_infant_slots * 100.0 / total_slots,
      empty_eden_slots * 100.0 / total_slots,
      tomb_slots * 100.0 / total_slots
  end
}
