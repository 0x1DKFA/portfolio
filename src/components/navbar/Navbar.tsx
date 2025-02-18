import { motion } from 'framer-motion';
import Sidebar from '../Sidebar/Sidebar';
import './navbar.scss';

const Navbar = () => {
    return <div className='navbar'>
        <Sidebar />
        <div className='wrapper'>
            <motion.span
                initial={{ opacity: 0, scale: 0.5 }}
                animate={{ opacity: 1, scale: 1 }}
                transition={{ duration: 0.5 }}
            >
                Rohit Kumar
            </motion.span>
            <span className='online-presence'>
                <a href="https://github.com/Wild-Soul">
                    <img src="/github.svg" alt="github" />
                </a>
                <a href="https://www.linkedin.com/in/rohitkk074/">
                    <img src="/linkedin.svg" alt="linkedin" />
                </a>
                <a href="https://stackoverflow.com/users/12846701/cicada">
                    <img src="/stackoverflow.svg" alt="stackoverflow" />
                </a>
            </span>
        </div>
    </div>
}

export default Navbar;
