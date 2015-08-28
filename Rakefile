require 'bundler/gem_tasks'
require 'rake/extensiontask'
require 'rspec/core/rake_task'
require 'yard'
require_relative 'mini_portile_with_autogen'

RSpec::Core::RakeTask.new(:spec)
task :spec => [:compile]

desc 'Run tests'
task :default => [:spec]

task :libuv do
  MiniPortileWithAutogen.new("libuv", ENV["LIBUV_VERSION"] || "1.7.3").tap do |recipe|
    recipe.files = ["https://github.com/libuv/libuv/archive/v#{recipe.version}.tar.gz"]
    recipe.target = File.join(File.dirname(__FILE__), "ports")
    recipe.configure_options << "--disable-shared"
    recipe.configure_options << "--enable-static"
    recipe.configure_options << "CFLAGS=-fPIC #{ENV['CFLAGS']}".shellescape

    checkpoint = "#{recipe.target}/#{recipe.name}-#{recipe.version}-#{recipe.host}.installed"
    unless File.exist? checkpoint
      recipe.cook
      FileUtils.touch checkpoint
    end
    recipe.activate
  end
end

Rake::ExtensionTask.new('rbuv') do |ext|
  ext.lib_dir = File.join('lib', 'rbuv')
  ext.source_pattern = "*.{c,h}"
end

YARD::Rake::YardocTask.new do |t|
  t.files   = ['lib/**/*.rb', 'ext/**/*.c']
end
