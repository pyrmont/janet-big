(declare-project
  :name "big"
  :author "Andrew Chambers"
  :license "MIT"
  :url "https://github.com/andrewchambers/janet-big"
  :repo "git+https://github.com/andrewchambers/janet-big.git")

(defn pkg-config [what]
  (def f (file/popen (string "pkg-config " what)))
  (def v (->>
           (file/read f :all)
           (string/trim)
           (string/split " ")))
  (unless (zero? (file/close f))
    (error "pkg-config failed!"))
  v)

(declare-native
    :name "big"
    :cflags (pkg-config "libtommath --cflags")
    :lflags (pkg-config "libtommath --libs")
    :source ["big.c"])
