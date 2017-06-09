![FFF](./assets/fff.jpg)

## FFF

Fuzzy File Finder.


## Installation

    $ git clone https://github.com/delta4d/fff
    $ cd fff
    $ make
    $ make install


## Usage

| shortcut                                        | meaning                        |
|-------------------------------------------------|--------------------------------|
| <kbd>C-C</kbd>, <kbd>C-Q</kbd>                  | quit **without** selection     |
| <kbd>ENTER</kbd>                                | quit **with** selection        |
| <kbd>C-J</kbd>, <kbd>C-N</kbd>, <kbd>DOWN</kbd> | next matched item              |
| <kbd>C-K</kbd>, <kbd>C-P</kbd>, <kbd>UP</kbd>   | previous matched item          |
| <kbd>LEFT</kbd>, <kbd>C-B</kbd>                 | move cursor 1 position left    |
| <kbd>RIGHT</kbd>, <kbd>C-F</kbd>                | move cursor 1 position right   |
| <kbd>C-A</kbd>                                  | move cursor to the beginning   |
| <kbd>C-E</kbd>                                  | move cursor to the end         |
| <kbd>BACKSPACE</kbd>                            | delete character before cursor |
| <kbd>C-U</kbd>                                  | clear the current line         |


[![asciicast](https://asciinema.org/a/124087.png)](https://asciinema.org/a/124087)


## Scoring

The fuzzy search algorithm is pretty straight forward.
It is just a 2-pointer traversal on pattern and text string.
`O(n+m)` time complexity.


## License

MIT


## Contribution

Feel free to file an issue, or make a PR.
