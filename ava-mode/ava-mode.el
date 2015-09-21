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
   (cons (regexp-opt (list
                      "extern" "Extern" "EXTERN"
                      "fun" "Fun" "FUN"
                      "macro" "Macro" "MACRO"
                      "namespace" "import"
                      "ret"
                      "alias" "Alias" "ALIAS")
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
    (modify-syntax-entry ?$   "."     st)
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

(defvar ava-mode-indent-level 2
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

(define-derived-mode ava-mode fundamental-mode "Avalanche"
  "Major mode for editing Avalanche code."
  :abbrev-table nil ; use parent table
  (setq font-lock-defaults
        '(ava-font-lock-keywords
          nil t
          (("_-" . "w")) nil))
  (setq comment-start "; ")
  (setq comment-start-skip ";+\\s*"))

(provide 'ava-mode)

;;; ava-mode.el ends here
