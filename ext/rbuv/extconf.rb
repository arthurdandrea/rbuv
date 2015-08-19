require 'mkmf'
require 'rbconfig'
$CFLAGS.gsub! "-Wno-error=shorten-64-to-32", ""
# $CFLAGS << " -Wdeclaration-after-statement "
MakeMakefile::CONFIG["CC"] = "gcc-4.6"
dir_config('uv')
if have_library('uv', 'uv_run', ['uv.h'])
  have_header('ruby/thread.h')
  have_func('rb_thread_call_without_gvl', 'ruby/thread.h')

  ##
  # Adds -DRBUV_DEBUG for compilation
  # To turn it on, use: --with-debug or --enable-debug
  #
  if debug_arg = with_config("debug") || enable_config("debug")
    debug_def = "-DRBUV_DEBUG"
    $defs.push(debug_arg.is_a?(String) ? "#{debug_def}=#{debug_arg}" : debug_def) unless $defs.include?(/\A#{debug_arg}/)
  end

  create_header
  create_makefile('rbuv/rbuv')
end
