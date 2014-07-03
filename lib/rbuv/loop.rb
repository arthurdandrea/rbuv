module Rbuv
  class Loop
    # creates a {Rbuv::Tcp} associate with this loop
    # @return [Rbuv::Tcp] a fresh {Rbuv::Tcp} instance
    def tcp
      Tcp.new(self)
    end

    # creates a {Rbuv::Timer} associate with this loop
    # @return [Rbuv::Timer] a fresh {Rbuv::Timer} instance
    def timer
      Timer.new(self)
    end

    # creates a {Rbuv::Signal} associate with this loop
    # @return [Rbuv::Signal] a fresh {Rbuv::Signal} instance
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

    # Tries to close every handle associated with this loop. It may call {#run}.
    # @return [self] itself
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
