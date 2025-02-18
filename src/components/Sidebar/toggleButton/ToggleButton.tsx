const ToggleButton = ({ setOpen }: { setOpen: React.Dispatch<React.SetStateAction<boolean>> }) => {
    return <button onClick={() => setOpen((prev) => !prev)}>
        Button
    </button>
};

export default ToggleButton
