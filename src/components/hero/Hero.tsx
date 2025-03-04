import { motion } from 'motion/react'
import './hero.css';
import Speech from './Speech';

const socialVariants = {
    initial: {
        y: -100,
        opacity: 0,
    },
    animate: {
        y: 0,
        opacity: 1,
        transition: {
            duration: 1,
            staggerChildren: 0.2,
        }
    }
}

const Hero = () => {
    return (
        <div className="hero">
            <div className='hSection left'>
                <motion.h1 className='hTitle'
                    initial={{ y: -100, opacity: 0 }}
                    animate={{ y: 0, opacity: 1 }}
                    transition={{ duration: 1 }}
                >
                    Hey There,
                    <br />
                    <span>I'm Rohit</span>
                </motion.h1>

                <motion.a
                    animate={{ opacity: [0, 1, 0] }}
                    transition={{
                        repeat: Infinity,
                        duration: 4,
                        ease: "easeInOut"
                    }}
                    href='#services'
                >
                    <svg
                        width="50px"
                        height="50px"
                        viewBox="0 0 24 24"
                        fill="none"
                        xmlns="http://www.w3.org/2000/svg"
                    >
                        <path
                            d="M5 9C5 5.13401 8.13401 2 12 2C15.866 2 19 5.13401 19 9V15C19 18.866 15.866 22 12 22C8.13401 22 5 18.866 5 15V9Z"
                            stroke="white"
                            strokeWidth="1"
                        />
                        <path
                            d="M12 5V8"
                            stroke="white"
                            strokeWidth="1"
                            strokeLinecap="round"
                        />
                    </svg>
                </motion.a>
            </div>

            <div className='hSection right'>
                <motion.div variants={socialVariants} initial="initial" animate="animate" className='social'>
                    <motion.a variants={socialVariants} href='/'>
                        <img src='/github.svg' alt='github' />
                    </motion.a>
                    <motion.a variants={socialVariants} href='/'>
                        <img src='/linkedin.svg' alt='linkedin' />
                    </motion.a>
                    <motion.a variants={socialVariants} href='/'>
                        <img src='/stackoverflow.svg' alt='stackoverflow' />
                    </motion.a>
                    <motion.div variants={socialVariants} className='letsConnectContainer'>
                        <div className='letsConnectText'>Let's connect!</div>
                    </motion.div>
                </motion.div>

                <Speech />

                <motion.a
                    href='/#contact'
                    className='contactLink'
                    animate={{
                        x: [100, 0],
                        opacity: [0, 1],
                    }}
                    transition={{
                        duration: 1,
                    }}
                >
                    <motion.div 
                        className='contactButton'
                        animate={{
                            rotate: [0, 360],
                        }}
                        transition={{
                            duration: 10,
                            repeat: Infinity,
                            ease: "linear",
                        }}
                    >
                        <svg viewBox="0 0 200 200" width="150" height="150">
                            <circle cx="100" cy="100" r="90" fill="pink" />
                            <path
                                id="innerCirclePath"
                                fill="none"
                                d="M 100,100 m -60,0 a 60,60 0 1,1 120,0 a 60,60 0 1,1 -120,0"
                            />
                            <text className="circleText">
                                <textPath href="#innerCirclePath">Hire Now •</textPath>
                            </text>
                            <text className="circleText">
                                <textPath href="#innerCirclePath" startOffset="44%">
                                    Contact Me •
                                </textPath>
                            </text>
                        </svg>
                        <div className="arrow">
                            <svg
                                xmlns="http://www.w3.org/2000/svg"
                                viewBox="0 0 24 24"
                                width="50"
                                height="50"
                                fill="none"
                                stroke="black"
                                strokeWidth="2"
                            >
                                <line x1="6" y1="18" x2="18" y2="6" />
                                <polyline points="9 6 18 6 18 15" />
                            </svg>
                        </div>
                    </motion.div>
                </motion.a>
            </div>
        </div>
    )
}

export default Hero;
