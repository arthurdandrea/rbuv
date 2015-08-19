require 'spec_helper'
require 'shared_context/loop'
require 'pp'
require 'fiber'

describe Rbuv::GetaddrinfoRequest do
  include_context Rbuv::Loop

  let(:expected_result) {
    expected_result = Socket.getaddrinfo("google.com", "ftp")
    expected_result.each do |arr|
      arr.delete_at(3)
    end
    expected_result
  }

  it "works" do
    pending "so far it only works on OS X" unless RUBY_PLATFORM.downcase.include? 'darwin'
    block = double("block")
    expect(block).to receive(:call).once.with(expected_result, nil)
    subject = Rbuv::GetaddrinfoRequest.new("google.com", "ftp", loop) do |result, error|
      block.call(result, error)
    end
    loop.run
  end

  it "is not garbage collected" do
    block = double("block")
    expect(block).to receive(:call).once.with(kind_of(Array), nil)
    subject = Rbuv::GetaddrinfoRequest.new("google.com", "ftp", loop) do |result, error|
      block.call(result, error)
    end
    subject = nil
    GC.start
    loop.run
  end
end
