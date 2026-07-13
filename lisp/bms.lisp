(defun bms-loop () {
    (var v-cell-support (eq (first (trap (get-bms-val 'bms-v-cell-min))) 'exit-ok))
    (loopwhile t {
        ;; can-id == -1 is normal before a sample and when no smart BMS exists.
        ;; Ask the BMS layer first so an enabled integration can start its
        ;; request/timeout state; disabled integration remains a no-op.
        (var bms-enabled (ext-bms))
        ;; BLDC updates BMS fields from separate CAN packets. Trap the complete
        ;; snapshot so a transient missing field drops one sample, not the loop.
        (trap (if (and bms-enabled (>= (get-bms-val 'bms-can-id) 0)) {
            (var msg-age (get-bms-val 'bms-msg-age))
            (var temp-max (get-bms-val 'bms-temp-cell-max))
            (var temp-min temp-max)
            (var temp-fet -281)

            (if v-cell-support {
                (if (= (get-bms-val 'bms-data-version) 1) {
                    (setq temp-min (get-bms-val 'bms-temps-adc 1))
                    (setq temp-fet (get-bms-val 'bms-temps-adc 3))
                })
                (var v-min (get-bms-val 'bms-v-cell-min))
                (var v-max (get-bms-val 'bms-v-cell-max))
                (ext-bms v-min v-max temp-min temp-max temp-fet msg-age)
            } {
                (var num-cells (get-bms-val 'bms-cell-num))
                (if (> num-cells 0) {
                    (var v-min (get-bms-val 'bms-v-cell 0))
                    (var v-max v-min)
                    (looprange i 1 num-cells {
                        (var cell-v (get-bms-val 'bms-v-cell i))
                        (if (< cell-v v-min) (setq v-min cell-v))
                        (if (> cell-v v-max) (setq v-max cell-v))
                    })
                    (ext-bms v-min v-max temp-min temp-max temp-fet msg-age)
                })
            })
        }))
        (sleep 0.2)
    })
})
