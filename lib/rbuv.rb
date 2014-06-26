require 'rbuv/rbuv'
require 'rbuv/version'
require 'rbuv/timer'
require 'rbuv/signal'
require 'rbuv/loop'

module Rbuv
  class << self

    def run_loop
      Loop.default.run
    end

    def stop_loop
      Loop.default.stop
    end

    alias stop stop_loop

    def run(&block)
      Loop.default.run(&block)
    end

    def run_block
      Timer.start(0, 0) { yield }
      Loop.default.run_once
    end

  end
end
