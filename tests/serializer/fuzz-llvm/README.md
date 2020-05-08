This is an [LLVM LibFuzzer](https://llvm.org/docs/LibFuzzer.html)-based test.

A typical setup could look like this:

```
cd libstud-json
bdep init -C @fuzz cc config.cxx=clang++ config.cxx.coptions="-g -O3 -fsanitize=address,undefined,fuzzer-no-link"
b ../libstud-json-fuzz/libstud-json/tests/serializer/fuzz-llvm/ # Directory may not exist at this point
cd ../libstud-json-fuzz/libstud-json/tests/serializer/fuzz-llvm/
mkdir corpus
./driver corpus/
```

The serializer's driver does not support starting from an empty corpus so the
corpus has to be pre-initialized. It is highly recommended to start with as
many high-quality samples as possible. The following repositories are a good
starting point:
  * The `test_parsing/` directory from
    [JSONTestSuite](https://github.com/nst/JSONTestSuite)
  * The `json/corpus/` directory from
    [go-fuzz-corpus](https://github.com/dvyukov/go-fuzz-corpus/tree/master)

It would also be wise to include a basic multi-value input such as this:

```
123
"abc"
true
false
null
[]
{}
```

The serializer's fuzz driver uses a custom input format. The included
`convert` utility can be used to convert valid JSON input to this custom
format. The following shell command can be used to convert an entire
directory of JSON files:

```
for f in corpus-json/*
do
    ./convert $f corpus/`basename $f` || rm corpus/`basename $f`
done
```
