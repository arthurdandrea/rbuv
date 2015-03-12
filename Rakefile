require "bundler/gem_tasks"
require 'rake/extensiontask'
require 'rspec/core/rake_task'
require 'yard'

task :clean => ['deps:clean']

RSpec::Core::RakeTask.new(:spec)
task :spec => [:compile]

desc 'Run tests'
task :default => [:spec]

Rake::ExtensionTask.new('rbuv') do |ext|
  ext.lib_dir = File.join('lib', 'rbuv')
  ext.source_pattern = "*.{c,h}"
end

YARD::Rake::YardocTask.new do |t|
  t.files   = ['lib/**/*.rb', 'ext/**/*.c']
end
