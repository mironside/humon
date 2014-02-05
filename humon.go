package main

import (
	"fmt"
	"io/ioutil"
	"os"
	"strings"
	"unicode"
	"unicode/utf8"
)

/*
func ScanJsonTokens(data []byte, atEOF bool) (advance int, token []byte, err error) {
	// Skip leading spaces.
	start := 0
	for width := 0; start < len(data); start += width {
		var r rune
		r, width = utf8.DecodeRune(data[start:])
		if !unicode.IsSpace(r) {
			break
		}
	}
	if atEOF && len(data) == 0 {
		return 0, nil, nil
	}

	r, width := utf8.DecodeRune(data[start:])
	switch r {
	case '{', '}', '[', ']', ':', ',':
		return start + width, data[start : start+width], nil
	case '"':
		// String
		// Scan until close quote, marking end of string.
		var prev rune
		for width, i := 0, start+1; i < len(data); i += width {
			var r rune
			r, width = utf8.DecodeRune(data[i:])
			if r == '"' && prev != '\\' {
				return i + width, data[start : i+1], nil
			}
			prev = r
		}
	default:
		// Word
		// Scan until space, marking end of word.
		for width, i := 0, start; i < len(data); i += width {
			var r rune
			r, width = utf8.DecodeRune(data[i:])
			if unicode.IsSpace(r) || r == '{' || r == '}' || r == '[' || r == ']' || r == ':' || r == ',' {
				return i, data[start:i], nil
			}
		}
	}
	// If we're at EOF, we have a final, non-empty, non-terminated word. Return it.
	if atEOF && len(data) > start {
		return len(data), data[start:], nil
	}
	// Request more data.
	return 0, nil, nil
}

func parseString(data []byte) (int, string, error) {
	end := 0
	for end = 1; end < len(data); {
		r, width := utf8.DecodeRune(data[end:])
		end += width
		if r == '"' && data[end-1] != '\\' {
			break
		}
	}
	return end, string(data[:end]), nil
}

func parseTrue(data []byte) (int, string, error) {
	if string(data[:4]) == "true" {
		return 4, "true", nil
	} else {
		return 0, "", errors.New("expected 'true'")
	}

}

func parseFalse(data []byte) (int, string, error) {
	if string(data[:5]) == "false" {
		return 5, "false", nil
	} else {
		return 0, "", errors.New("expected 'false'")
	}
}

func parseNull(data []byte) (int, string, error) {
	if string(data[:4]) == "null" {
		return 4, "null", nil
	} else {
		return 0, "", errors.New("expected 'null'")
	}
}

func parseNumber(data []byte) (int, string, error) {
	// @todo: fix!
	end := 0
	for end = 0; end < len(data); {
		r, width := utf8.DecodeRune(data[end:])
		if (r >= '0' && r <= '9') || r == '.' || r == 'e' || r == 'E' || r == '+' || r == '-' {
			end += width
		} else {
			break
		}
	}
	return end, string(data[:end]), nil
}

func parseObject(data []byte) (int, map[string]interface{}, error) {
	return 0, map[string]interface{}{}, nil
}

func parseArray(data []byte) (int, []interface{}, error) {
	fmt.Print("ARRAY!", data, "\n")
	result := []interface{}{}
	end := 0
	for end = 1; end < len(data); {
		r, width := utf8.DecodeRune(data[end:])
		if r == ']' {
			end += width
			break
		} else {
			size, v, _ := parseValue(data[end:])
			result = append(result, v)
			end += size
		}
	}
	return end, result, nil
}

func parseValue(data []byte) (int, interface{}, error) {
	var v interface{}
	var err error
	var size int

	i := 0
	for i = 0; i < len(data); {
		r, w := utf8.DecodeRune(data[i:])
		if r != ' ' && r != '\t' && r != '\r' && r != '\n' {
			break
		}
		i += w
	}
	fmt.Print(i, "\n")

	r, _ := utf8.DecodeRune(data[i:])
	switch r {
	case '"':
		size, v, err = parseString(data[i:])
	case '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '-':
		size, v, err = parseNumber(data[i:])
	case '{':
		size, v, err = parseObject(data[i:])
	case '[':
		size, v, err = parseArray(data[i:])
	case 't':
		size, v, err = parseTrue(data[i:])
	case 'f':
		size, v, err = parseFalse(data[i:])
	case 'n':
		size, v, err = parseNull(data[i:])
	}
	fmt.Print(v, err, "\n")
	if err != nil {
		return 0, "", err
	}
	return i + size, v, nil
}

func convertJson(file *os.File) string {

	const (
		None = iota
		Object_Open
		Object_Key
		Object_Value
		Object_Close
		Array_Open
		Array_Element
		Array_Close
	)

	scanner := bufio.NewScanner(file)
	scanner.Split(ScanJsonTokens)

	var result bytes.Buffer

	stack := make([]int, 0, 100)
	stack = append(stack, Object_Open)

	for scanner.Scan() {
		text := scanner.Text()
		fmt.Print(text, "\n")

		nextState := None
		switch text {
		case "{":
			nextState = Object_Open
		case "}":
			stack[len(stack)-1] = Object_Close
		case "[":
			nextState = Array_Open
		case "]":
			stack[len(stack)-1] = Array_Close
		default:
			if text[0] != '"' {
				if _, err := strconv.ParseInt(text, 10, 64); err == nil {
				} else if _, err := strconv.ParseFloat(text, 64); err == nil {
				} else if text == "true" || text == "false" || text == "null" {
				} else {
					text = "\"" + text + "\""
				}
			}
		}

		switch stack[len(stack)-1] {
		case Array_Open:
			stack[len(stack)-1] = Array_Element
		case Array_Element:
			result.WriteString(",")
		case Array_Close:
			stack = stack[:len(stack)-1]
		case Object_Open:
			stack[len(stack)-1] = Object_Key
		case Object_Key:
			stack[len(stack)-1] = Object_Value
			result.WriteString(":")
		case Object_Value:
			stack[len(stack)-1] = Object_Key
			result.WriteString(",")
		case Object_Close:
			stack = stack[:len(stack)-1]
		}

		if nextState > 0 {
			stack = append(stack, nextState)
		}
		result.WriteString(text)
	}

	return "{" + result.String() + "}"
}

func ScanHumonTokens(data []byte, atEOF bool) (advance int, token []byte, err error) {
	// Skip leading spaces.
	start := 0
	for width := 0; start < len(data); start += width {
		var r rune
		r, width = utf8.DecodeRune(data[start:])
		if !unicode.IsSpace(r) {
			break
		}
	}
	if atEOF && len(data) == 0 {
		return 0, nil, nil
	}

	r, width := utf8.DecodeRune(data[start:])
	switch r {
	case '{', '}', '[', ']':
		return start + width, data[start : start+width], nil
	case '"':
		// String
		// Scan until close quote, marking end of string.
		var prev rune
		for width, i := 0, start+1; i < len(data); i += width {
			var r rune
			r, width = utf8.DecodeRune(data[i:])
			if r == '"' && prev != '\\' {
				return i + width, data[start : i+1], nil
			}
			prev = r
		}
	default:
		// Word
		// Scan until space, marking end of word.
		for width, i := 0, start; i < len(data); i += width {
			var r rune
			r, width = utf8.DecodeRune(data[i:])
			if unicode.IsSpace(r) || r == '{' || r == '}' || r == '[' || r == ']' {
				return i, data[start:i], nil
			}
		}
	}
	// If we're at EOF, we have a final, non-empty, non-terminated word. Return it.
	if atEOF && len(data) > start {
		return len(data), data[start:], nil
	}
	// Request more data.
	return 0, nil, nil
}

func convertHumon(file *os.File) string {

	const (
		None = iota
		Object_Open
		Object_Key
		Object_Value
		Object_Close
		Array_Open
		Array_Element
		Array_Close
	)

	scanner := bufio.NewScanner(file)
	scanner.Split(ScanHumonTokens)

	var result bytes.Buffer

	stack := make([]int, 0, 100)
	stack = append(stack, Object_Open)

	for scanner.Scan() {
		text := scanner.Text()

		nextState := None
		switch text {
		case "{":
			nextState = Object_Open
		case "}":
			stack[len(stack)-1] = Object_Close
		case "[":
			nextState = Array_Open
		case "]":
			stack[len(stack)-1] = Array_Close
		default:
			if text[0] != '"' {
				if _, err := strconv.ParseInt(text, 10, 64); err == nil {
				} else if _, err := strconv.ParseFloat(text, 64); err == nil {
				} else if text == "true" || text == "false" || text == "null" {
				} else {
					text = "\"" + text + "\""
				}
			}
		}

		switch stack[len(stack)-1] {
		case Array_Open:
			stack[len(stack)-1] = Array_Element
		case Array_Element:
			result.WriteString(",")
		case Array_Close:
			stack = stack[:len(stack)-1]
		case Object_Open:
			stack[len(stack)-1] = Object_Key
		case Object_Key:
			stack[len(stack)-1] = Object_Value
			result.WriteString(":")
		case Object_Value:
			stack[len(stack)-1] = Object_Key
			result.WriteString(",")
		case Object_Close:
			stack = stack[:len(stack)-1]
		}

		if nextState > 0 {
			stack = append(stack, nextState)
		}
		result.WriteString(text)
	}

	return "{" + result.String() + "}"
}

func dumpValue(v interface{}, depth int) {
	switch t := v.(type) {
	case map[string]interface{}:
		if depth > 0 {
			fmt.Print("{\n")
		}
		for k, v := range t {
			for i := 0; i < depth; i++ {
				fmt.Print("\t")
			}
			fmt.Print(k, " ")
			dumpValue(v, depth+1)
			fmt.Print("\n")
		}
		if depth > 0 {
			for i := 0; i < depth-1; i++ {
				fmt.Print("\t")
			}
			fmt.Print("}")
		}
	case []interface{}:
		fmt.Print("[")
		first := true
		for _, e := range t {
			if !first {
				fmt.Print(" ")
			}
			first = false
			dumpValue(e, depth)
		}
		fmt.Print("]")
	case int, float64, bool:
		fmt.Print(t)
	case nil:
		fmt.Print("null")
	case string:
		b, _ := json.Marshal(t)
		s := string(b)
		if !strings.ContainsAny(s, " \t\\[]{}") {
			s = s[1 : len(s)-1]
		}
		fmt.Print(s)
	default:
	}
}

func TestJsonTokenizer() {
	file, _ := os.Open("C:/Users/colsen/Dropbox/test.json")
	data, _ := ioutil.ReadAll(file)
	fmt.Print(data, "\n")
	_, v, err := parseValue(data)
	fmt.Print(v, err)
}
*/

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
	input     string
	start     int
	pos       int
	width     int
	token     tokenType
	tokenText string
}

func (l *lexer) run() (tokenType, string) {
	l.token = None
	l.tokenText = ""
	for state := lexWhitespace; state != nil; {
		state = state(l)
	}
	return l.token, l.tokenText
}

func (l *lexer) next() rune {
	if l.pos >= len(l.input) {
		l.width = 0
		return 0
	}
	r, w := utf8.DecodeRuneInString(l.input[l.pos:])
	l.width = w
	l.pos += l.width
	return r
}

func (l *lexer) peek() rune {
	r := l.next()
	l.pos -= l.width
	return r
}

func (l *lexer) skip() {
	l.start = l.pos
}

func (l *lexer) backup() {
	l.pos -= l.width
}

func (l *lexer) emit(token tokenType) {
	//fmt.Print(l.input[l.start:l.pos], "\n")
	l.tokenText = l.input[l.start:l.pos]
	l.token = token
	l.start = l.pos
}

func (l *lexer) error(msg string) {
	fmt.Print("Lex Error: ", msg, "\n")
}

func (l *lexer) accept(valid string) bool {
	if strings.IndexRune(valid, l.next()) >= 0 {
		return true
	}
	l.backup()
	return false
}

func (l *lexer) acceptRun(valid string) bool {
	accept := false
	for strings.IndexRune(valid, l.next()) >= 0 {
		accept = true
	}
	l.backup()
	return accept
}

func lexWhitespace(l *lexer) stateFn {
	l.acceptRun(" \t\r\n")
	l.skip()
	return lexText
}

func lexText(l *lexer) stateFn {
	switch {
	case l.accept("["):
		l.emit(LBracket)
		return nil
	case l.accept("]"):
		l.emit(RBracket)
		return nil
	case l.accept("{"):
		l.emit(LBrace)
		return nil
	case l.accept("}"):
		l.emit(RBrace)
		return nil
	case l.accept(","):
		l.emit(Comma)
		return nil
	case l.accept(":"):
		l.emit(Colon)
		return nil
	case l.accept("\""):
		return lexString
	default:
		return lexName
	}
}

func lexString(l *lexer) stateFn {
	for {
		switch l.next() {
		case '"':
			l.emit(Value)
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
		if r == 0 || unicode.IsSpace(r) || r == '{' || r == '}' || r == '[' || r == ']' || r == ':' || r == ',' {
			l.backup()
			if l.pos > l.start {
				l.emit(Value)
			}
			return nil
		}
	}
}

func lex(input string) {
	l := &lexer{
		input: input,
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
	tokenText       string
	lexer           *lexer
	backupToken     tokenType
	backupTokenText string
}

func (p *parser) next() tokenType {
	if p.backupToken != None {
		p.token = p.backupToken
		p.tokenText = p.backupTokenText
		p.backupToken = None
		p.backupTokenText = ""
	} else {
		p.token, p.tokenText = p.lexer.run()
	}
	return p.token
}

func (p *parser) accept(t tokenType) bool {
	if p.next() == t {
		return true
	}
	p.backup()
	return false
}

func (p *parser) backup() {
	p.backupToken = p.token
	p.backupTokenText = p.tokenText
	p.token = None
	p.tokenText = ""
}

func parse(input string) interface{} {
	p := &parser{
		lexer: &lexer{
			input: input,
		},
	}
	return parseValue(p)
}

func parseObject(p *parser) map[string]interface{} {
	result := map[string]interface{}{}
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
			result[k] = v
		}
	}
}

func parseArray(p *parser) []interface{} {
	result := []interface{}{}
	p.accept(LBracket)
	first := true
	for {
		if p.accept(RBracket) {
			return result
		}
		if first || p.accept(Comma) {
			result = append(result, parseValue(p))
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
		return parseObject(p)
	case LBracket:
		return parseArray(p)
	}
	return nil
}

func main() {
	file, _ := os.Open("C:/Users/colsen/Dropbox/test.json")
	data, _ := ioutil.ReadAll(file)
	//lex(string(data))
	r := parse(string(data))
	fmt.Print(r)
	return

	/*
		if len(os.Args) < 3 || (os.Args[1] != "json2humon" && os.Args[1] != "humon2json") {
			fmt.Print("usage: humon.exe [json2humon|humon2json] <filename>\n")
			return
		}

		file, err := os.Open(os.Args[2])
		if err != nil {
			fmt.Print("File Not Found\n")
			return
		}

		start := time.Now()
		switch os.Args[1] {
		case "json2humon":
			var d map[string]interface{}
			data, _ := ioutil.ReadAll(file)
			json.Unmarshal(data, &d)
			dumpValue(d, 0)
		case "humon2json":
			text := convertHumon(file)
			fmt.Fprint(os.Stdout, text)
		}
		end := time.Now()
		fmt.Fprint(os.Stderr, "\n", end.Sub(start))
	*/
}
