# ==========================================
#   CMock Project - Automatic Mock Generation for C
#   Copyright (c) 2007 Mike Karlesky, Mark VanderVoord, Greg Williams
#   [Released under MIT License. Please refer to license.txt for details]
# ========================================== 

class CMockFileWriter

  attr_reader :config

  def initialize(config)
    @config = config
  end

  def create_file(filename)
    raise "Where's the block of data to create?" unless block_given?
    full_file_name_temp = "#{@config.mock_path}/#{filename}.new"
    full_file_name_done = "#{@config.mock_path}/#{filename}"
    File.open(full_file_name_temp, 'w') do |file|
      yield(file, filename)
    end
    update_file(full_file_name_done, full_file_name_temp)
  end
  
  private ###################################
  
  def update_file(dest, src)
    require 'fileutils'
    FileUtils.rm(dest) if (File.exist?(dest))
    FileUtils.cp(src, dest)
    FileUtils.rm(src)
  end
end
