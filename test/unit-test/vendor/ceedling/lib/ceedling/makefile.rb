
# modified version of Rake's provided make-style dependency loader
# customizations:
#  (1) handles windows drives in paths -- colons don't confuse task demarcation
#  (2) handles spaces in directory paths

module Rake

  # Makefile loader to be used with the import file loader.
  class MakefileLoader

    # Load the makefile dependencies in +fn+.
    def load(fn)
      open(fn) do |mf|
        lines = mf.read
        lines.gsub!(/#[^\n]*\n/m, "") # remove comments
        lines.gsub!(/\\\n/, ' ')      # string together line continuations into single line
        lines.split("\n").each do |line|
          process_line(line)
        end
      end
    end

    private

    # Process one logical line of makefile data.
    def process_line(line)
      # split on presence of task demaractor followed by space (i.e don't get confused by a colon in a win path)
      file_tasks, args = line.split(/:\s/)

      return if args.nil?

      # split at non-escaped space boundary between files (i.e. escaped spaces in paths are left alone)
      dependents = args.split(/\b\s+/)
      # replace escaped spaces and clean up any extra whitespace
      dependents.map! { |path| path.gsub(/\\ /, ' ').strip }

      file_tasks.strip.split.each do |file_task|
        file file_task => dependents
      end
    end
  end

  # Install the handler
  Rake.application.add_loader('mf', MakefileLoader.new)
end
