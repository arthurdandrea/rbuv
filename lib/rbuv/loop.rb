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

    def run_with_block(&block)
      one_shot_timer(0, &block) if block
      run_without_block
    end
    alias_method :run_without_block, :run
    alias_method :run, :run_with_block

    def dispose
      return if self.handles.empty?
      self.handles.each do |handle|
        handle.close unless handle.closing?
      end
      run
      raise RuntimeError unless self.handles.empty?
    end

    private

    def one_shot_timer(timeout, &block)
      timer = Timer.new(self)
      timer.start(timeout, 0) do
        timer.close
        block.call
      end
    end
  end
end
