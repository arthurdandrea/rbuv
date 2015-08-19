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

    # Tries to close every handle associated with this loop. It may call {#run}.
    # @return [self] itself
    def dispose
      return self if self.handles.empty?
      self.handles.each do |handle|
        handle.close unless handle.closing?
      end
      run_nowait
      raise RuntimeError unless self.handles.empty?
      self
    end

    # Runs the loop using {#run} then ensure that {#dispose} is called
    # @yield (see #run)
    # @yieldparam loop (see #run)
    # @return (see #run)
    def run_and_dispose(&block)
      run(&block)
    ensure
      dispose
    end
  end
end
