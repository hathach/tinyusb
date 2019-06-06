class PreprocessinatorExtractor
  def extract_base_file_from_preprocessed_expansion(filepath)
    # preprocessing by way of toolchain preprocessor expands macros, eliminates
    # comments, strips out #ifdef code, etc.  however, it also expands in place
    # each #include'd file.  so, we must extract only the lines of the file
    # that belong to the file originally preprocessed

    # iterate through all lines and alternate between extract and ignore modes
    # all lines between a '#'line containing file name of our filepath and the
    # next '#'line should be extracted

    base_name  = File.basename(filepath)
    not_pragma = /^#(?!pragma\b)/ # preprocessor directive that's not a #pragma
    pattern    = /^#.*(\s|\/|\\|\")#{Regexp.escape(base_name)}/
    found_file = false # have we found the file we care about?

    lines = []
    File.readlines(filepath).each do |line|
      if found_file and not line.match(not_pragma)
        lines << line
      else
        found_file = false
      end

      found_file = true if line.match(pattern)
    end

    return lines
  end
end
