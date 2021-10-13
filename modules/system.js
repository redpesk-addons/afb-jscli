
export function system(command)
{
	if (!Array.isArray(command))
		command = String(command).split(/ +/);
	os.exec.apply(this, command);
}
