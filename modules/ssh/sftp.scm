;;; sftp.scm -- Procedures for working with SFTP.

;; Copyright (C) 2015 Artyom V. Poptsov <poptsov.artyom@gmail.com>
;;
;; This file is a part of Guile-SSH.
;;
;; Guile-SSH is free software: you can redistribute it and/or
;; modify it under the terms of the GNU General Public License as
;; published by the Free Software Foundation, either version 3 of the
;; License, or (at your option) any later version.
;;
;; Guile-SSH is distributed in the hope that it will be useful, but
;; WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
;; General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with Guile-SSH.  If not, see <http://www.gnu.org/licenses/>.


;;; Commentary:

;;; Code:

(define-module (ssh sftp)
  #:export (sftp-session?
            make-sftp-session
            sftp-get-session))

(define (make-sftp-session session)
  "Make a new SFTP session using a SSH SESSION."
  (%make-sftp-session session))

(define (sftp-session? x)
  "Return #t if X is a SFTP session, #f otherwise."
  (%sftp-session? x))

(define (sftp-init sftp-session)
  "Initialize a SFTP-SESSION with the server.  Throw 'guile-ssh-error'
exception on an error, return value is undefined."
  (%gssh-sftp-init sftp-session))

(define (sftp-get-session sftp-session)
  "Get the parent SSH session for a SFTP-SESSION."
  (%gssh-sftp-get-session sftp-session))


(load-extension "libguile-ssh" "init_sftp_session")

;;; sftp-session.scm ends here.
