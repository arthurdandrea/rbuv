module Rbuv
  class Loop
    def tcp
      Tcp.new(self)
    end

    def timer
      Timer.new(self)
    end

    def signal
      Signal.new(self)
    end

    alias_method :c_run, :run
    private :c_run
    def run(&block)
      Timer.one_shot(self) { yield self } if block_given?
      c_run
    end

    alias_method :c_run_once, :run_once
    private :c_run_once
    def run_once(&block)
      Timer.one_shot(self) { yield self } if block_given?
      c_run_once
    end

    alias_method :c_run_nowait, :run_nowait
    private :c_run_nowait
    def run_nowait(&block)
      Timer.one_shot(self) { yield self } if block_given?
      c_run_nowait
    end

    def dispose
      return self if self.handles.empty?
      self.handles.each do |handle|
        handle.close unless handle.closing?
      end
      run
      raise RuntimeError unless self.handles.empty?
      self
    end
  end
end
