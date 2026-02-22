if exists("b:current_syntax")
    finish
endif

" Keywords
syn keyword vyneKeyword use fn return if else while through loop unique collect filter extern
syn keyword vyneType    Int64 Float64 String Array

" Comments
syn match vyneComment "//.*$"
syn region vyneComment start="/\*" end="\*/"

" Strings
syn region vyneString start='"' end='"' skip='\\"'

hi def link vyneKeyword Keyword
hi def link vyneType    Type
hi def link vyneComment Comment
hi def link vyneString  String

let b:current_syntax = "vyne"