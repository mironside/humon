package main

import (
	"bytes"
	"fmt"
	"io/ioutil"
	"os"
	"runtime/pprof"
	"strconv"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"
)

func indent(depth int) *bytes.Buffer {
	buffer := bytes.Buffer{}
	for i := 0; i < depth; i++ {
		buffer.WriteString("  ")
	}
	return &buffer
}

type stateFn func(*lexer) stateFn

type tokenType int

const (
	None tokenType = iota
	Value
	LBracket
	RBracket
	LBrace
	RBrace
	Colon
	Comma
)

type lexer struct {
	input     []byte
	start     int
	pos       int
	width     int
	token     tokenType
	tokenText []byte
	json      bool
	length    int
}

func (l *lexer) run() (tokenType, []byte) {
	l.token = None
	l.tokenText = nil
	for state := lexWhitespace; state != nil; {
		state = state(l)
	}
	return l.token, l.tokenText
}

func (l *lexer) next() rune {
	if l.pos >= l.length {
		l.width = 0
		return 0
	}
	r, w := utf8.DecodeRune(l.input[l.pos:])
	l.width = w
	l.pos += l.width
	return r
}

func (l *lexer) skip() {
	l.start = l.pos
}

func (l *lexer) backup() {
	l.pos -= l.width
}

func (l *lexer) emit(token tokenType) {
	l.tokenText = l.input[l.start:l.pos]
	l.token = token
	l.start = l.pos
}

func (l *lexer) error(msg string) {
	fmt.Print("Lex Error: ", msg, "\n")
}

func lexWhitespace(l *lexer) stateFn {
	for r := l.next(); ; r = l.next() {
		if r != ' ' && r != '\t' && r != '\r' && r != '\n' {
			break
		}
	}

	l.backup()
	l.skip()
	return lexText
}

func lexText(l *lexer) stateFn {
	r := l.next()

	switch r {
	case '[':
		l.emit(LBracket)
		return nil
	case ']':
		l.emit(RBracket)
		return nil
	case '{':
		l.emit(LBrace)
		return nil
	case '}':
		l.emit(RBrace)
		return nil
	case ',':
		l.emit(Comma)
		return nil
	case ':':
		l.emit(Colon)
		return nil
	case '"':
		return lexString
	default:
		return lexName
	}
}

func lexString(l *lexer) stateFn {
	for {
		switch l.next() {
		case '"':
			s := l.input[l.start:l.pos]

			unquote := true
			invalid := []rune{'\\', '{', '}', '[', ']', ' ', '\t'}
			for _, i := range invalid {
				if bytes.IndexRune(s, i) >= 0 {
					unquote = false
					break
				}
			}

			if len(s) > 2 && s[0] == '"' && s[len(s)-1] == '"' && unquote {
				l.start += 1
				l.pos -= 1
				l.emit(Value)
				l.start -= 1
				l.pos += 1
			} else {
				l.emit(Value)
			}
			return nil
		case '\r', '\n':
			l.error("Newline in string")
			return nil
		case 0:
			l.error("EOF in string")
			return nil
		}
	}
}

func lexName(l *lexer) stateFn {
	for {
		r := l.next()
		if r == 0 || unicode.IsSpace(r) || r == '{' || r == '}' || r == '[' || r == ']' || (l.json && (r == ':' || r == ',')) {
			l.backup()
			if l.pos > l.start {
				l.emit(Value)
			}
			return nil
		}
	}
}

func lex(input []byte) {
	l := &lexer{
		input:  input,
		length: len(input),
	}
	for {
		token, _ := l.run()
		if token == None {
			break
		}
	}
}

type parseFn func(*parser) parseFn

type parser struct {
	token           tokenType
	tokenText       []byte
	lexer           *lexer
	backupToken     tokenType
	backupTokenText []byte
}

func (p *parser) next() tokenType {
	if p.backupToken != None {
		p.token = p.backupToken
		p.tokenText = p.backupTokenText
		p.backupToken = None
		p.backupTokenText = nil
	} else {
		p.token, p.tokenText = p.lexer.run()
	}
	return p.token
}

func (p *parser) accept(t tokenType) bool {
	v := p.next()
	if v == t {
		return true
	}
	p.backup()
	return false
}

func (p *parser) backup() {
	p.backupToken = p.token
	p.backupTokenText = p.tokenText
	p.token = None
	p.tokenText = []byte{}
}

func parseJson(input []byte) interface{} {
	p := &parser{
		lexer: &lexer{
			input:  input,
			length: len(input),
			json:   true,
		},
	}
	return parseValue(p)
}

func parseObject(p *parser) []Pair {
	result := make([]Pair, 0, 10)
	p.accept(LBrace)
	first := true
	for {
		if p.accept(RBrace) {
			return result
		}
		if first || p.accept(Comma) {
			p.accept(Value)
			k := p.tokenText
			p.accept(Colon)
			v := parseValue(p)
			result = append(result, Pair{key: k, value: v})
		}
		first = false
	}
}

func parseArray(p *parser) []interface{} {
	result := make([]interface{}, 0, 10)
	p.accept(LBracket)
	first := true
	for {
		if p.accept(RBracket) {
			return result
		}
		if first || p.accept(Comma) {
			v := parseValue(p)
			result = append(result, v)
		}
		first = false
	}
}

func parseValue(p *parser) interface{} {
	t := p.next()
	switch t {
	case Value:
		return p.tokenText
	case LBrace:
		p.backup()
		return parseObject(p)
	case LBracket:
		p.backup()
		return parseArray(p)
	}
	return nil
}

func parseHumon(input []byte) interface{} {
	p := &parser{
		lexer: &lexer{
			input:  input,
			length: len(input),
		},
	}
	return parseHumonValue(p)
}

func parseHumonObject(p *parser) []Pair {
	result := make([]Pair, 0, 10)
	p.accept(LBrace)
	for {
		if p.accept(RBrace) {
			return result
		}
		p.accept(Value)
		k := p.tokenText
		v := parseHumonValue(p)
		result = append(result, Pair{key: k, value: v})
	}
}

func parseHumonArray(p *parser) []interface{} {
	result := make([]interface{}, 0, 10)
	p.accept(LBracket)
	for {
		if p.accept(RBracket) {
			return result
		}
		result = append(result, parseHumonValue(p))
	}
}

func parseHumonValue(p *parser) interface{} {
	t := p.next()
	switch t {
	case Value:
		return p.tokenText
	case LBrace:
		p.backup()
		return parseHumonObject(p)
	case LBracket:
		p.backup()
		return parseHumonArray(p)
	}
	return nil
}

func printHumonValue(buffer *bytes.Buffer, v interface{}, depth int) {
	switch t := v.(type) {
	case []byte:
		buffer.Write(t)
	case []Pair:
		buffer.WriteString("{\n")
		l := 0
		for _, p := range t {
			k := p.key
			if len(k) > l {
				l = len(k)
			}
		}
		for _, p := range t {
			buffer.Write(indent(depth + 1).Bytes())
			k := p.key
			buffer.Write(k)
			switch p.value.(type) {
			case []Pair:
				buffer.WriteString(" ")
			default:
				m := len(k)
				for i := 0; i < l-m+1; i++ {
					buffer.WriteString(" ")
				}
			}
			buffer.WriteString(" ")
			printHumonValue(buffer, p.value, depth+1)
			buffer.WriteString("\n")
		}
		buffer.Write(indent(depth).Bytes())
		buffer.WriteString("}")
	case []interface{}:
		buffer.WriteString("[")
		first := true
		for _, e := range t {
			//buffer.Write(indent(depth + 1).Bytes())
			if !first {
				buffer.WriteString(" ")
			}
			first = false
			printHumonValue(buffer, e, depth+1)
			//buffer.WriteString("\n")
		}
		//buffer.Write(indent(depth).Bytes())
		buffer.WriteString("]")
	}
}

func printJsonValue(buffer *bytes.Buffer, v interface{}, depth int) {
	switch t := v.(type) {
	case []byte:
		_, err := strconv.ParseFloat(string(t), 64)
		quote := err != nil && (len(t) < 2 || (t[0] != '"' && t[len(t)-1] != '"'))
		if quote {
			buffer.WriteString("\"")
		}
		buffer.Write(t)
		if quote {
			buffer.WriteString("\"")
		}
	case []Pair:
		buffer.WriteString("{")
		first := true
		for _, p := range t {
			if !first {
				buffer.WriteString(",")
			}
			first = false
			buffer.WriteString("\n")
			buffer.Write(indent(depth + 1).Bytes())
			if len(p.key) < 2 || (p.key[0] != '"' && p.key[len(p.key)-1] != '"') {
				buffer.WriteString("\"")
			}
			buffer.Write(p.key)
			if len(p.key) < 2 || (p.key[0] != '"' && p.key[len(p.key)-1] != '"') {
				buffer.WriteString("\"")
			}
			buffer.WriteString(": ")
			printJsonValue(buffer, p.value, depth+1)
		}
		buffer.WriteString("\n")
		buffer.Write(indent(depth).Bytes())
		buffer.WriteString("}")
	case []interface{}:
		buffer.WriteString("[")
		first := true
		for _, e := range t {
			if !first {
				buffer.WriteString(",")
			}
			buffer.WriteString("\n")
			first = false
			//buffer.Write(indent(depth + 1).Bytes())
			printJsonValue(buffer, e, depth+1)
		}
		buffer.WriteString("\n")
		//buffer.Write(indent(depth).Bytes())
		buffer.WriteString("]")
	}
}

type Pair struct {
	key   []byte
	value interface{}
}

// @todo: output primitive lines
// @todo: use bufio for efficient output
// @todo: handle invalid and unexpected tokens
func main() {
	f, _ := os.Create("humon.cpuprofile")
	pprof.StartCPUProfile(f)
	defer pprof.StopCPUProfile()

	file, _ := os.Open(os.Args[1])
	data, _ := ioutil.ReadAll(file)

	start := time.Now()
	if strings.HasSuffix(os.Args[1], ".humon") {
		r := parseHumon(data)
		buffer := &bytes.Buffer{}
		buffer.Grow(len(data))
		printJsonValue(buffer, r, 0)
		fmt.Print(buffer.String())
	} else if strings.HasSuffix(os.Args[1], ".json") {
		r := parseJson(data)
		buffer := &bytes.Buffer{}
		buffer.Grow(len(data))
		printHumonValue(buffer, r, 0)
		fmt.Print(buffer.String())
	}

	f, _ = os.Create("humon.memprofile")
	pprof.WriteHeapProfile(f)

	end := time.Now()
	dt := end.Sub(start)
	fmt.Print(dt)
}
