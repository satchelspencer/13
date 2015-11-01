class database{
	public:
		database(int chipSelect);
		String insert(String data);
		String remove(String id);
		String find(String id);
		String update(String id, String data);
		bool exists(String path);
	private:
		String uniqueId();
		bool writeFile(String path, String data);
};
