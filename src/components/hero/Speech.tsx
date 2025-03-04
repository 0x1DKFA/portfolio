import { TypeAnimation } from 'react-type-animation';
import { motion } from 'motion/react';

const Speech = () => {
    return (
        <motion.div
            className="bubbleContainer"
            animate={{ opacity: [0, 1] }}
            transition={{ duration: 1 }}
        >
            <div className="bubble">
                <TypeAnimation
                    sequence={[
                        1000,
                        "Hello, I'm a Full-stack software engineer with experience in developing web applications and APIs.",
                        1000,
                        "Experienced in concurrency control, system optimization for performance, cost, and efficiency."
                    ]}
                    wrapper='span'
                    speed={50}
                    deletionSpeed={70}
                    repeat={Infinity}
                />
            </div>
            <img src="/vite.svg" />
        </motion.div>
    )
}

export default Speech;
