require 'test/unit'
require 'gctools/gcprof'

class TestGCProf < Test::Unit::TestCase
  def teardown
    GCProf.after_gc_hook = nil
  end

  def test_after_gc_hook
    n = 0
    GCProf.after_gc_hook = proc{ n += 1 }
    GC.start
    Object.new
    assert_equal 1, n
  end
end
