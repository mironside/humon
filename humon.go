package main

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
	"io/ioutil"
	"os"
	"strconv"
	"strings"
	"time"
	"unicode"
	"unicode/utf8"
)

func ScanTokens(data []byte, atEOF bool) (advance int, token []byte, err error) {
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
		for width, i := 0, start+1; i < len(data); i += width {
			var r rune
			r, width = utf8.DecodeRune(data[i:])
			if r == '"' {
				return i + width, data[start : i+1], nil
			}
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

func convertDJSON(file *os.File) string {

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
	scanner.Split(ScanTokens)

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
		if !strings.ContainsAny(s, " \t\\") {
			s = s[1 : len(s)-1]
		}
		fmt.Print(s)
	default:
	}
}

func main() {
	file, err := os.Open(os.Args[1])
	if err != nil {
		fmt.Print("File Not Found\n")
		return
	}

	var d map[string]interface{}
	data, _ := ioutil.ReadAll(file)
	json.Unmarshal(data, &d)
	dumpValue(d, 0)
	return

	start := time.Now()
	text := convertDJSON(file)
	end := time.Now()
	fmt.Print(text)
	fmt.Fprint(os.Stderr, "\n", end.Sub(start))
}
