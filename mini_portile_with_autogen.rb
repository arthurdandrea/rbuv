require 'mini_portile'

class MiniPortileWithAutogen < MiniPortile
  def configure
    return if configured?
    execute('autogen', %Q(sh autogen.sh)) unless File.exist?(File.join(work_path, "configure"))
    super
  end
end
