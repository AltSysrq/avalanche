;;; ava-mode --- Major mode for editing Avalanche programs

;;;  Copyright (C) 2015 Jason Lingle

;; Author: Jason Lingle <jason@lin.gl>
;; Version: 0.1
;; Keywords: languages, processes, tools

;; This program is free software: you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation, either version 3 of the License, or
;; (at your option) any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with this program.  If not, see <http://www.gnu.org/licenses/>.

;;; Commentary:
;;

;;; Code:

;;;###autoload
(add-to-list 'auto-mode-alist '("\\.ava\\'" . ava-mode))
(defconst ava-font-lock-keywords
  (list
   (cons (regexp-opt
          (list
           "alias" "Alias" "ALIAS"
           "break"
           "collect" "collecting"
           "continue"
           "catch"
           "defer"
           "do"
           "each"
           "else"
           "extern" "Extern" "EXTERN"
           "finally"
           "for"
           "fun" "Fun" "FUN"
           "goto"
           "in"
           "if"
           "import"
           "macro" "Macro" "MACRO"
           "namespace"
           "on-any-bad-format"
           "pasta"
           "reqmod" "reqpkg"
           "ret"
           "throw" "throw-err" "throw-fmt"
           "try"
           "unreachable"
           "until"
           "while"
           "workaround" "workaround-undefined")
          'symbols)
         'font-lock-keyword-face)
   (cons "\\$\\(\\s_\\|\\w\\)+\\$?" 'font-lock-variable-name-face)))

(defvar ava-mode-syntax-table
  (let ((st (make-syntax-table)))
    (modify-syntax-entry 59   "<"     st) ; ?; confuses emacs's syntax highlighter
    (modify-syntax-entry ?\n  ">"     st)
    (modify-syntax-entry ?\"  "|"     st)
    (modify-syntax-entry ?`   "|"     st)
    (modify-syntax-entry ?\\  "\\"    st)
    (modify-syntax-entry ?$   "'"     st)
    (modify-syntax-entry ?!   "_"     st)
    (modify-syntax-entry ?#   "_"     st)
    (modify-syntax-entry ?%   "_"     st)
    (modify-syntax-entry ?&   "_"     st)
    (modify-syntax-entry (cons ?* ?,) "_" st)
    (modify-syntax-entry ?.   "_"     st)
    (modify-syntax-entry ?/   "_"     st)
    (modify-syntax-entry ?:   "_"     st)
    (modify-syntax-entry (cons ?< ?@) "_" st)
    (modify-syntax-entry ?^   "_"     st)
    (modify-syntax-entry ?_   "_"     st)
    (modify-syntax-entry ?|   "_"     st)
    (modify-syntax-entry ?~   "_"     st)
    (modify-syntax-entry (cons 128 65535) "_" st)
    st)
  "Syntax table for `ava-mode'.")

(defvar ava-indent-level 2
  "Size of an indentation level in Avalanche source code.")

(defun ava-delete-ws-backward ()
  "Like c-hungry-delete-backward."
  (interactive)
  (let ((start (point)))
    (skip-chars-backward " \t\r\n\f")
    (delete-region (point) start)))

(defun ava-delete-ws-forward ()
  "Like c-hungry-delete-forward."
  (interactive)
  (let ((start (point)))
    (skip-chars-forward " \t\r\n\f")
    (delete-region (point) start)))

(defun ava-find-start-of-enclosing-sexp ()
  "Locates the character address of the character starting the sexp enclosing
  point.

Returns nil if it fails to find anything."
  (let ((start (point)))
    (save-excursion
      (beginning-of-defun)
      (nth 1 (parse-partial-sexp (point) start)))))

(defun ava-is-paren-enclosure-p (pos)
  "Returns whether the given position represents the start of a
parenthesis-like enclosure.

A parenthesis-like enclosure is one where all line breaks are implicitly
continuations, whereas others are indented as if new statements are started
with each line break if nothing indicates otherwise.

pos is a value like `ava-find-start-of-enclosing-sexp' returns."
  (and pos
       (< pos (point-max))
       (eq ?\( (char-after pos))))

(defun ava-line-is-prefix-continuation ()
  "Returns whether the line containing point is a prefix continuation.

That is, whether the first non-whitespace character on the line (if any) is a
backslash followed by a space."
  (save-excursion
    (back-to-indentation)
    (and (eq ?\\ (char-after))
         (progn
           (forward-char)
           (eq 32 (char-after))))))

; Originally from thingatpt.el
(defun ava-in-string-p ()
  "Return non-nil if point is in an Avalanche string."
  (let ((orig (point)))
    (save-excursion
      (beginning-of-defun)
      (nth 3 (parse-partial-sexp (point) orig)))))

(defun ava-char-syntax-after (pos)
  "Return the `char-syntax' of `char-after', or nil if there is `char-after' is
nil."
  (let ((ch (char-after pos)))
    (if ch (char-syntax ch) nil)))

(defun ava-is-close-paren-line-p ()
  "Return non-nil if the current line begins with close-parenthesis-like
characters."
  (save-excursion
    (back-to-indentation)
    (eq ?\) (ava-char-syntax-after (point)))))

(defun ava-indent-close-paren ()
  "Indent current line on the assumption that it begins with closing
parentheses.

The final consecutive closing parenthisis from the beginning of the line is
found, then the line is reindented to the indentation of the line which starts
the group that the last closing parenthesis closes."
  (save-excursion
    (back-to-indentation)
    (while (eq ?\) (ava-char-syntax-after (point)))
      (forward-char)
      (skip-chars-forward " \t"))
    (condition-case nil
        (indent-line-to
         (save-excursion
           (backward-sexp)
           (current-indentation)))
        (error nil))))

(defun ava-is-multiline-expression-p (enclosure-start)
  "Return non-nil if point is in a continuation line or if the enclosure is
parenthesis-like."
  (or (ava-is-paren-enclosure-p enclosure-start)
      (ava-line-is-prefix-continuation)))

(defun ava-column-of-first-sexp-in-enclosure-same-line ()
  "Return the columnn of the start of the first sexp in the enclosure which
starts at point, or nil if there is no such sexp."
  (let ((start (line-beginning-position)))
    (save-excursion
      (forward-char 1)
      (and (ava-try-forward-sexp 1) (ava-try-forward-sexp -1)
           (= start (line-beginning-position))
           (current-column)))))

(defun ava-indent-according-to-enclosure (enclosure-start)
  "Indent the current line based on the indentation of the line containing
enclosure-start plus `ava-indent-level'.

If enclosure-start is nil, indent to column 0."
  (indent-line-to
   (if enclosure-start
       (save-excursion
         (goto-char enclosure-start)
         (or (ava-column-of-first-sexp-in-enclosure-same-line)
             (+ (current-indentation) ava-indent-level)))
     0)))

(defun ava-try-forward-sexp (n)
  "Call `forward-sexp' with the given argument.

If it returns normally, return non-nil. Otherwise, leave point where it was
before this call and return nil."
  (condition-case nil
      (progn (forward-sexp n) t)
    (error nil)))

(defun ava-string-is-bareword (str)
  "Return whether str is recognised as a single bareword according to the
Avalanche Syntax I rules."
  (not (string-match "[\0- \"`(){};]\\|\\\\\\|\\[\\|\\]" str)))

(defun ava-word-looks-like-operator (word)
  "Return whether the given word (ie, a bareword) looks like a conventional
Avalanche operator."
  (and word (> (length word) 0)
       (or
        (ava-string-is-bareword word)
        (eq ?` (elt word (- (length word) 1))))
       (/= ?$ (elt word (- (length word) 1)))
       (let ((cat (get-char-code-property (elt word (- (length word) 1))
                                          'general-category)))
         (or (eq 'Sm cat) (eq 'So cat) (eq 'Sc cat) (eq 'Sk cat)
             (eq 'Pc cat) (eq 'Pd cat) (eq 'Ps cat) (eq 'Pc cat)
             (eq 'Pi cat) (eq 'Pf cat) (eq 'Po cat)))))

(defun ava-prev-sexp-is-operator ()
  "Return whether the sexp before point looks like a conventional Avalanche
operator.

Return nil if there is no sexp before point."
  (save-excursion
    (and (ava-try-forward-sexp -1)
         (let ((sexp (thing-at-point 'sexp t)))
           (ava-word-looks-like-operator sexp)))))

(defun ava-find-multiline-indent-control-point (enclosure-start)
  "Move point to the start of the `control point' of a multi-line expression.

If point cannot be moved off of the current line by traversing sexps, do not
move point and return nil.

If point is transfered to the first sexp in the current enclosure, leave it at
the beginning of that sexp and return `absolute'.

Otherwise, position point at the start of the first sexp (moving backwards)
which begins at its line indentation. If point ends up on a continuation line,
as per `ava-is-multiline-expression-p' and is preceded by another sexp,
return `relative'. Otherwise, return `absolute'.

This assumes it is invoked at the beginning of a line and that it is not within
a string."
  (if (not (ava-try-forward-sexp -1)) nil
    (while (and (ava-try-forward-sexp -1)
                (/= (current-indentation) (current-column))))
    (if (and (save-excursion (ava-try-forward-sexp -1))
             (ava-is-multiline-expression-p enclosure-start))
        'relative 'absolute)))

(defun ava-sexp-has-sexp-on-same-line ()
  "Return non-nil if the current sexp is followed by at least one sexp on the
same line.

This assumes point is already positioned at the start of the 'current' sexp.

When it returns non-nil, returns the starting column of the sexp it found."
  (save-excursion
    (let ((start (line-beginning-position)))
      (and (ava-try-forward-sexp 2)
           (ava-try-forward-sexp -1)
           (= start (line-beginning-position))
           (current-column)))))

(defun ava-choose-multiline-indent-absolute
    (prev-is-operator implicit-continuation)
  "Return a cons indicating the indetation relative to an absolute control
point.

Point is assumed to be at the start of a sexp definining an absolute
indentation control point, as per `ava-find-multiline-indent-control-point'.

The car of the cons is the indentation for a leading backslash, if any. The cdr
is the indentation for the main body of the code being indented.

prev-is-operator indicates whether the last sexp preceding the line being
indented looks like an operator."
  (cons (current-column)
        (let ((first-parm (ava-sexp-has-sexp-on-same-line)))
          ; If the control point has arguments starting on the same line and
          ; the line is not operator-terminated, indent to align to the first
          ; argument.
          ;
          ; Otherwise, indent one level greater than the control point if
          ; explicit continuations are necessary, or to the same level as the
          ; control point if they are implicit.
          (if (and first-parm (not prev-is-operator))
              first-parm
            (+ (current-column)
               (if implicit-continuation 0 ava-indent-level))))))

(defun ava-choose-multiline-indent-relative ()
  "Return a cons indicating the indentation relative to a relative control
point.

Point is assumed to be at the start of a sexp defining a relative indentation
control point, as per `ava-find-multiline-indent-control-point'.

The car of the cons is the indentation for a leading backslash, if any. The cdr
is the indentation for the main body of the code being indented."
  (cons (current-column)
        (if (eq ?\\ (char-after))
            (save-excursion
              (and
               (ava-try-forward-sexp 2)
               (ava-try-forward-sexp -1))
              (current-column))
          (current-column))))

(defun ava-adjust-indentation-after-backslash (target)
  "Reposition the first non-horizontal-whitespace character after the current
character to be at the given target column.

This will delete characters as necessary, possibly including the character
originally at point.

Point is left at the character that was moved."
  (forward-char 1)
  (skip-chars-forward "[ \t]")
  (let ((col (current-column)))
    (cond
     ((< col target) (indent-to target))
     ((> col target)
      (delete-region
       (point)
       (progn (move-to-column target)
              (point)))))))

(defun ava-apply-multiline-indent (indent)
  "Adjust indentation of the current line according to a cons from
`ava-choose-multiline-indent-absolute' or
  `ava-choose-multiline-indent-relative'.

This will delete a leading backslash on the current line if it would be
indented the same amount as the body."
  (if (ava-line-is-prefix-continuation)
      (progn (indent-line-to (car indent))
             (ava-adjust-indentation-after-backslash (cdr indent)))
    (indent-line-to (cdr indent))))

(defun ava-indent-multiline-expression (enclosure-start)
  "Adjust indentation of the current line assuming it is a continuation of a
multiline expression.

Point is assumed to be at the beginning of the line."
  (let ((chosen-indent
         (save-excursion
           (let ((prev-is-operator (ava-prev-sexp-is-operator))
                 (control (ava-find-multiline-indent-control-point
                           enclosure-start)))
             (cond
              ((eq 'absolute control)
               (ava-choose-multiline-indent-absolute
                prev-is-operator
                (ava-is-paren-enclosure-p enclosure-start)))
              ((eq 'relative control)
               (ava-choose-multiline-indent-relative)))))))
    (if chosen-indent
        (ava-apply-multiline-indent chosen-indent)
      (ava-indent-according-to-enclosure enclosure-start))))

(defun ava-indent-line ()
  "Indent current line as Avalanche code.

This is intended to be used as a value for `indent-line-function'."
  (interactive)
  (save-excursion
    (move-beginning-of-line nil)
    (let ((enclosure-start (ava-find-start-of-enclosing-sexp)))
      (cond
       ((ava-in-string-p) 'noindent)
       ((ava-is-close-paren-line-p) (ava-indent-close-paren))
       ((ava-is-multiline-expression-p enclosure-start)
        (ava-indent-multiline-expression enclosure-start))
       ((ava-indent-according-to-enclosure enclosure-start)))))
  ; If point ended up before indentation, move it to the indentation point
  (when (< (current-column) (current-indentation))
    (back-to-indentation)))

(defun ava-electric-brace (arg)
  "Insert character and correct indentation.

This function only reindents the line if the character it inserted was a
consecutive electric character at the start of the line, so that it does not
continuously override manual indentation."
  (interactive "p")
  (self-insert-command arg)
  (if (<= (save-excursion (while (and (char-before)
                                      (or (= ?\) (char-syntax (char-before)))
                                          (= ?   (char-syntax (char-before)))))
                            (backward-char))
                          (point))
          (line-beginning-position))
      (ava-indent-line)))

(defvar ava-mode-map
  (let ((map (make-sparse-keymap)))
    (define-key map "}" 'ava-electric-brace)
    (define-key map "]" 'ava-electric-brace)
    (define-key map ")" 'ava-electric-brace)
    (define-key map "\C-c\177" 'ava-delete-ws-backward)
    (define-key map "\C-c\C-d" 'ava-delete-ws-forward)
    map)
  "Keymap used in `ava-mode'.")

(define-derived-mode ava-mode prog-mode "Avalanche"
  "Major mode for editing Avalanche code."
  :abbrev-table nil ; use parent table
  (setq font-lock-defaults
        '(ava-font-lock-keywords
          nil t
          (("_-" . "w")) nil))
  (set (make-local-variable 'comment-start) "; ")
  (set (make-local-variable 'comment-end) "")
  (set (make-local-variable 'comment-start-skip) ";+\\s*")
  (set (make-local-variable 'indent-line-function) 'ava-indent-line)

  (set (make-local-variable 'dabbrev-case-fold-search) nil)
  (set (make-local-variable 'dabbrev-case-replace) nil)
  (set (make-local-variable 'dabbrev-abbrev-skip-leading-regexp) "[$]")
  (set (make-local-variable 'dabbre-abbrev-char-regexp) nil)
  (set (make-local-variable 'open-paren-in-column-0-is-defun-start) nil)

  (set (make-local-variable 'parse-sexp-ignore-comments) t))

(provide 'ava-mode)

;;; ava-mode.el ends here
