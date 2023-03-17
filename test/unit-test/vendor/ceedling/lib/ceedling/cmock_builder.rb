require 'cmock'

class CmockBuilder

  attr_accessor :cmock

  def setup
    @cmock = nil
  end

  def manufacture(cmock_config)
    @cmock = CMock.new(cmock_config)
  end

end
