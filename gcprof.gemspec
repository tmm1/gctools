Gem::Specification.new do |s|
  s.name = 'gcprof'
  s.version = '0.1.0'
  s.summary = 'rgengc profiler for ruby 2.1+'
  s.description = 'gc debugger and profiler for rgengc in ruby 2.1'

  s.homepage = 'https://github.com/tmm1/gcprof'
  s.authors = 'Aman Gupta'
  s.email   = 'aman@tmm1.net'
  s.license = 'MIT'

  s.files = `git ls-files`.split("\n")
  s.extensions = 'ext/extconf.rb'
  s.add_development_dependency 'rake-compiler'
end
