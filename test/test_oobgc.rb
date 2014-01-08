require 'test/unit'
require 'gctools/oobgc'

class TestOOBGC < Test::Unit::TestCase
  def setup
    GC::OOB.setup
  end

  def test_oob_sweep
    GC.start(immediate_sweep: false)
    assert_equal false, GC.latest_gc_info(:immediate_sweep)

    before = GC.stat(:heap_swept_slot)
    assert_equal true, GC::OOB.run
    assert_operator GC.stat(:heap_swept_slot), :>, before
  end

  def test_oob_mark
    oob = 0
    before = GC.count

    20.times do
      (10_000 + rand(25_000)).times{ Object.new }
      oob += 1 if GC::OOB.run
    end

    assert_equal 0, GC.count - before - oob
  end

  def test_oob_major_mark
  end
end
