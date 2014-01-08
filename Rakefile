task :default => :test

# ==========================================================
# Packaging
# ==========================================================

GEMSPEC = eval(File.read('gctools.gemspec'))

require 'rubygems/package_task'
Gem::PackageTask.new(GEMSPEC) do |pkg|
end

# ==========================================================
# Ruby Extension
# ==========================================================

require 'rake/extensiontask'
Rake::ExtensionTask.new('gcprof', GEMSPEC) do |ext|
  ext.ext_dir = 'ext/gcprof'
  ext.lib_dir = 'lib/gctools'
end
Rake::ExtensionTask.new('oobgc', GEMSPEC) do |ext|
  ext.ext_dir = 'ext/oobgc'
  ext.lib_dir = 'lib/gctools'
end
task :build => :compile

# ==========================================================
# Testing
# ==========================================================

require 'rake/testtask'
Rake::TestTask.new 'test' do |t|
  t.test_files = FileList['test/test_*.rb']
end
task :test => :build
